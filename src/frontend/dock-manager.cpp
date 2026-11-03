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
    shutting_down_ = false;
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
        AddControlDockToDocksMenu();

        // Register all already-known filter states (collected before FINISHED_LOADING)
        for (auto& pair : active_states_) {
            control_dock_->AddFilterState(pair.second);
        }
    }
}

// ─── Docks Menu Helpers ───────────────────────────────────────────────────────

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
                if (a->text().contains("Surround Monitor", Qt::CaseInsensitive) ||
                    a->text().contains("Sorround Monitor", Qt::CaseInsensitive)) {
                    already_has = true;
                    break;
                }
            }
            if (!already_has) {
                QAction* toggle_act = monitor_dock_->toggleViewAction();
                toggle_act->setText("Surround Monitor");
                menu->addAction(toggle_act);
            }
            break;
        }
    }
}

void DockManager::AddControlDockToDocksMenu() {
    if (!main_window_ || !control_dock_) return;

    QMenuBar* menu_bar = main_window_->menuBar();
    if (!menu_bar) return;

    for (QAction* action : menu_bar->actions()) {
        QMenu* menu = action->menu();
        if (menu && (menu->title().contains("Dock", Qt::CaseInsensitive) ||
                     menu->objectName() == "docksMenu")) {
            bool already_has = false;
            for (QAction* a : menu->actions()) {
                if (a->text().contains("Surround Control", Qt::CaseInsensitive) ||
                    a->text().contains("Sorround Control", Qt::CaseInsensitive)) {
                    already_has = true;
                    break;
                }
            }
            if (!already_has) {
                QAction* toggle_act = control_dock_->toggleViewAction();
                toggle_act->setText("Surround Control");
                menu->addAction(toggle_act);
            }
            break;
        }
    }
}

void DockManager::Shutdown() {
    // Set the flag FIRST — blocks any queued callbacks from accessing docks/states
    shutting_down_ = true;

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
    if (!state || shutting_down_) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        active_states_[state->GetInstanceId()] = state;
    }

    // If the dock already exists (runtime filter add / scene collection switch),
    // add this state to the dock's dropdown on the main thread.
    if (control_dock_) {
        ControlDockWidget* dock = control_dock_;
        auto add_fn = [this, dock, state]() {
            if (shutting_down_ || !dock) return;
            dock->AddFilterState(state);
        };
        if (qApp && QThread::currentThread() != qApp->thread()) {
            QMetaObject::invokeMethod(qApp, add_fn, Qt::QueuedConnection);
        } else {
            add_fn();
        }
    }

    // If state was previously visible (persisted setting), show the dock.
    // Do NOT force setFloating here — OBS will restore the dock geometry.
    if (state->IsDockVisible() && main_window_) {
        QMetaObject::invokeMethod(qApp, [this, state]() {
            if (!shutting_down_) OpenControlDock(state);
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

    if (shutting_down_) return; // Docks are already being torn down — skip UI cleanup

    // UI cleanup (remove from dock dropdown) must happen on the main thread.
    ControlDockWidget* dock = control_dock_;
    auto run_ui_cleanup = [this, dock, state]() {
        if (shutting_down_ || !dock) return;
        dock->RemoveFilterState(state);
    };

    if (qApp && QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(qApp, run_ui_cleanup, Qt::QueuedConnection);
    } else {
        run_ui_cleanup();
    }
}

// ─── Dock Control ─────────────────────────────────────────────────────────────

void DockManager::OpenControlDock(FilterState* state) {
    if (!state || shutting_down_) return;

    auto run_open = [this, state]() {
        if (shutting_down_) return;

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
                AddControlDockToDocksMenu();
                for (auto& pair : active_states_) {
                    control_dock_->AddFilterState(pair.second);
                }
            }
        }

        // Switch to the requested filter
        control_dock_->SetActiveFilter(state);

        // Only force float if the dock is hidden (not yet positioned by user).
        // If it's already docked/positioned, just show it without disturbing layout.
        if (!control_dock_->isVisible()) {
            control_dock_->setFloating(true);
        }
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
        if (shutting_down_) return;
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

// ─── Audio Monitoring Refresh ─────────────────────────────────────────────────

void DockManager::RefreshAudioMonitoring() {
    if (shutting_down_) return;

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
        // Capture shutting_down_ by reference is unsafe (DockManager is static singleton,
        // so it outlives the lambda); read it inside the lambda instead.
        QTimer::singleShot(150, [this, filter_srcs]() {
            for (obs_source_t* filter_ctx : filter_srcs) {
                if (!shutting_down_) {
                    obs_source_t* parent = obs_filter_get_parent(filter_ctx);
                    if (parent && obs_source_active(parent)) {
                        uint32_t mon_type = obs_source_get_monitoring_type(parent);
                        if (mon_type != OBS_MONITORING_TYPE_NONE) {
                            obs_source_set_monitoring_type(parent, OBS_MONITORING_TYPE_NONE);
                            obs_source_set_monitoring_type(parent,
                                static_cast<enum obs_monitoring_type>(mon_type));
                        }
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
            AddControlDockToDocksMenu();
        }

        // Find any filter that was visible before OBS closed and restore the dock.
        // The dock geometry is restored automatically by OBS (via objectName).
        // We just need to make it visible and select the right filter.
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
