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

// ─── Init / Shutdown ─────────────────────────────────────────────────────────

void DockManager::Init(QMainWindow* main_window) {
    std::lock_guard<std::mutex> lock(mutex_);
    main_window_ = main_window;

    // Create Monitor dock (once)
    if (main_window_ && !monitor_dock_) {
        monitor_dock_ = new MonitorDockWidget(main_window_);
        obs_frontend_add_custom_qdock("obs_surround_monitor_dock", monitor_dock_);
        AddMonitorToDocksMenu();
    }

    // Create the single unified Sorround Control dock (once)
    if (main_window_ && !control_dock_) {
        control_dock_ = new ControlDockWidget(main_window_);
        obs_frontend_add_custom_qdock("obs_surround_control_dock", control_dock_);

        // Register all already-known filter states (collected before FINISHED_LOADING)
        for (auto& pair : active_states_) {
            control_dock_->AddFilterState(pair.second);
        }
    }
}

void DockManager::AddMonitorToDocksMenu() {
    if (!main_window_ || !monitor_dock_) return;

    QMenuBar* menu_bar = main_window_->menuBar();
    if (!menu_bar) return;

    for (QAction* action : menu_bar->actions()) {
        QMenu* menu = action->menu();
        if (menu && (menu->title().contains("Dock", Qt::CaseInsensitive) ||
                     menu->objectName() == "docksMenu")) {
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

    // Remove and delete the unified control dock
    if (control_dock_) {
        obs_frontend_remove_dock("obs_surround_control_dock");
        delete control_dock_;
        control_dock_ = nullptr;
    }

    // Remove and delete monitor dock
    if (monitor_dock_) {
        obs_frontend_remove_dock("obs_surround_monitor_dock");
        delete monitor_dock_;
        monitor_dock_ = nullptr;
    }

    active_states_.clear();
    main_window_ = nullptr;
}

// ─── Filter Registry ─────────────────────────────────────────────────────────

void DockManager::RegisterFilterState(FilterState* state) {
    if (!state) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        active_states_[state->GetInstanceId()] = state;
    }

    // If the dock already exists (runtime filter add / scene collection switch),
    // add this state to the dock's dropdown on the main thread.
    if (control_dock_) {
        ControlDockWidget* dock = control_dock_;
        auto add_fn = [dock, state]() {
            dock->AddFilterState(state);
        };
        if (qApp && QThread::currentThread() != qApp->thread()) {
            QMetaObject::invokeMethod(qApp, add_fn, Qt::QueuedConnection);
        } else {
            add_fn();
        }
    }

    // If state was previously visible (persisted setting), open/focus the dock
    if (state->IsDockVisible() && main_window_) {
        QMetaObject::invokeMethod(qApp, [this, state]() {
            OpenControlDock(state);
        }, Qt::QueuedConnection);
    }
}

void DockManager::UnregisterFilterState(FilterState* state) {
    if (!state) return;
    std::string id = state->GetInstanceId();

    // CRITICAL: Erase from active_states_ IMMEDIATELY (thread-safe under mutex)
    // so RefreshAudioMonitoring's 150ms timer never sees a dangling pointer.
    {
        std::lock_guard<std::mutex> lock(mutex_);
        active_states_.erase(id);
    }

    // UI cleanup (remove from dock dropdown) must happen on the main thread.
    ControlDockWidget* dock = control_dock_;
    auto run_ui_cleanup = [dock, state]() {
        if (dock) dock->RemoveFilterState(state);
    };

    if (qApp && QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(qApp, run_ui_cleanup, Qt::QueuedConnection);
    } else {
        run_ui_cleanup();
    }
}

// ─── Dock Control ────────────────────────────────────────────────────────────

void DockManager::OpenControlDock(FilterState* state) {
    if (!state) return;

    auto run_open = [this, state]() {
        // Ensure dock exists (should always be true after Init)
        if (!control_dock_) {
            if (!main_window_) {
                main_window_ = reinterpret_cast<QMainWindow*>(obs_frontend_get_main_window());
            }
            if (!main_window_) return;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                control_dock_ = new ControlDockWidget(main_window_);
                obs_frontend_add_custom_qdock("obs_surround_control_dock", control_dock_);
                for (auto& pair : active_states_) {
                    control_dock_->AddFilterState(pair.second);
                }
            }
        }

        // Switch to the requested filter and bring the dock to front
        control_dock_->SetActiveFilter(state);
        control_dock_->setFloating(true);
        control_dock_->show();
        control_dock_->raise();
        control_dock_->activateWindow();
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

// ─── Audio Monitoring Refresh ────────────────────────────────────────────────

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
                obs_source_t* ref = obs_source_get_ref(fc);
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
                obs_source_release(filter_ctx);
            }
        });
    } else {
        for (obs_source_t* fc : filter_srcs) obs_source_release(fc);
    }
}

// ─── Frontend Event Handler ───────────────────────────────────────────────────

void DockManager::OnFrontendEvent(enum obs_frontend_event event) {
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
        if (!main_window_) {
            Init(static_cast<QMainWindow*>(obs_frontend_get_main_window()));
        } else {
            AddMonitorToDocksMenu();
        }

        // Find any filter that was visible before OBS closed and restore the dock
        FilterState* state_to_restore = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& pair : active_states_) {
                if (pair.second && pair.second->IsDockVisible()) {
                    state_to_restore = pair.second;
                    break;
                }
            }
        }
        if (state_to_restore) {
            OpenControlDock(state_to_restore);
        }

    } else if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
        RefreshAudioMonitoring();
    } else if (event == OBS_FRONTEND_EVENT_EXIT) {
        Shutdown();
    }
}

} // namespace AeriaSurround
