#include "dock-manager.hpp"
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QThread>

namespace AeriaSurround {

DockManager& DockManager::Instance() {
    static DockManager s_instance;
    return s_instance;
}

void DockManager::Init(QMainWindow* main_window) {
    std::lock_guard<std::mutex> lock(mutex_);
    main_window_ = main_window;

    if (main_window_ && !monitor_dock_) {
        monitor_dock_ = new MonitorDockWidget(main_window_);
        obs_frontend_add_custom_qdock("obs_surround_monitor_dock", monitor_dock_);
        AddMonitorToDocksMenu();
    }
}

void DockManager::AddMonitorToDocksMenu() {
    if (!main_window_ || !monitor_dock_) return;

    QMenuBar* menu_bar = main_window_->menuBar();
    if (!menu_bar) return;

    for (QAction* action : menu_bar->actions()) {
        QMenu* menu = action->menu();
        if (menu && (menu->title().contains("Dock", Qt::CaseInsensitive) || menu->objectName() == "docksMenu")) {
            bool already_has = false;
            for (QAction* a : menu->actions()) {
                if (a->text().contains("Sorround Monitor", Qt::CaseInsensitive)) {
                    already_has = true;
                    break;
                }
            }
            if (!already_has) {
                QAction* toggle_act = monitor_dock_->toggleViewAction();
                toggle_act->setText("Sorround Monitor");
                menu->addAction(toggle_act);
            }
            break;
        }
    }
}

void DockManager::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Remove all control docks
    for (auto& pair : control_docks_) {
        if (pair.second) {
            obs_frontend_remove_dock(pair.first.c_str());
            delete pair.second;
        }
    }
    control_docks_.clear();

    // Remove monitor dock
    if (monitor_dock_) {
        obs_frontend_remove_dock("obs_surround_monitor_dock");
        delete monitor_dock_;
        monitor_dock_ = nullptr;
    }

    active_states_.clear();
    main_window_ = nullptr;
}

void DockManager::RegisterFilterState(FilterState* state) {
    if (!state) return;
    std::lock_guard<std::mutex> lock(mutex_);
    active_states_[state->GetInstanceId()] = state;

    // If the manager is already initialized (e.g. filter added at runtime or after
    // a scene collection switch) and the state wants its dock visible, restore it.
    if (main_window_ && state->IsDockVisible()) {
        QMetaObject::invokeMethod(qApp, [this, state]() {
            OpenControlDock(state);
        }, Qt::QueuedConnection);
    }
}

void DockManager::UnregisterFilterState(FilterState* state) {
    if (!state) return;
    std::string id = state->GetInstanceId();

    // CRITICAL: Always erase from active_states_ IMMEDIATELY and synchronously
    // (thread-safe under mutex) so that any pending timer callbacks (e.g.,
    // RefreshAudioMonitoring's 150ms QTimer) never encounter a dangling state pointer
    // after the caller does `delete ctx->state`.
    {
        std::lock_guard<std::mutex> lock(mutex_);
        active_states_.erase(id);
    }

    // Dock widget cleanup must run on the Qt main thread (UI thread).
    auto run_dock_cleanup = [this, id]() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = control_docks_.find(id);
        if (it != control_docks_.end()) {
            obs_frontend_remove_dock(id.c_str());
            delete it->second;
            control_docks_.erase(it);
        }
    };

    if (qApp && QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(qApp, run_dock_cleanup, Qt::QueuedConnection);
    } else {
        run_dock_cleanup();
    }
}

void DockManager::OpenControlDock(FilterState* state) {
    if (!state) return;
    std::string id = state->GetInstanceId();

    auto run_open = [this, state, id]() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = control_docks_.find(id);
        if (it != control_docks_.end() && it->second) {
            // Dock for this source already exists — bring it to front.
            it->second->setFloating(true);
            it->second->show();
            it->second->raise();
            it->second->activateWindow();
            return;
        }

        if (!main_window_) {
            main_window_ = reinterpret_cast<QMainWindow*>(obs_frontend_get_main_window());
        }

        // Create a new independent floating dock for this filter/source.
        auto* dock = new ControlDockWidget(state, main_window_);
        obs_frontend_add_custom_qdock(id.c_str(), dock);
        control_docks_[id] = dock;

        // Float immediately so each source gets its own visible window,
        // not hidden/tabbed behind another dock.
        dock->setFloating(true);
        dock->show();
        dock->raise();
        dock->activateWindow();
    };

    if (qApp && QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(qApp, run_open, Qt::QueuedConnection);
    } else {
        run_open();
    }
}

void DockManager::ShowMonitorDock() {
    auto run_show = [this]() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (monitor_dock_) {
            monitor_dock_->show();
            monitor_dock_->raise();
            monitor_dock_->activateWindow();
        }
    };

    if (qApp && QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(qApp, run_show, Qt::QueuedConnection);
    } else {
        run_show();
    }
}

void DockManager::RefreshAudioMonitoring() {
    // Snapshot valid filter sources with an addref so they stay alive
    // even if filters are destroyed within the 150ms delay window.
    std::vector<obs_source_t*> filter_srcs;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& pair : active_states_) {
            FilterState* state = pair.second;
            if (!state) continue;
            obs_source_t* fc = state->GetFilterContext();
            if (fc) {
                obs_source_t* ref = obs_source_get_ref(fc); // keep alive for 150ms window
                if (ref) filter_srcs.push_back(ref);
            }
        }
    }

    if (qApp && !filter_srcs.empty()) {
        QTimer::singleShot(150, [filter_srcs]() {
            for (obs_source_t* filter_ctx : filter_srcs) {
                obs_source_t* parent = obs_filter_get_parent(filter_ctx);
                if (parent && obs_source_active(parent)) {
                    uint32_t mon_type = obs_source_get_monitoring_type(parent);
                    if (mon_type != OBS_MONITORING_TYPE_NONE) {
                        obs_source_set_monitoring_type(parent, OBS_MONITORING_TYPE_NONE);
                        obs_source_set_monitoring_type(parent,
                            static_cast<enum obs_monitoring_type>(mon_type));
                    }
                }
                obs_source_release(filter_ctx); // release the ref we added above
            }
        });
    } else {
        // No timer needed — release any refs we added
        for (obs_source_t* fc : filter_srcs) {
            obs_source_release(fc);
        }
    }
}

void DockManager::OnFrontendEvent(enum obs_frontend_event event) {
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
        if (!main_window_) {
            Init(static_cast<QMainWindow*>(obs_frontend_get_main_window()));
        } else {
            AddMonitorToDocksMenu();
        }

        std::vector<FilterState*> states_to_open;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& pair : active_states_) {
                if (pair.second && pair.second->IsDockVisible()) {
                    states_to_open.push_back(pair.second);
                }
            }
        }
        for (FilterState* st : states_to_open) {
            OpenControlDock(st);
        }
    } else if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
        RefreshAudioMonitoring();
    } else if (event == OBS_FRONTEND_EVENT_EXIT) {
        Shutdown();
    }
}

} // namespace AeriaSurround
