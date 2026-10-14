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
}

void DockManager::UnregisterFilterState(FilterState* state) {
    if (!state) return;
    std::string id = state->GetInstanceId();

    auto run_cleanup = [this, id]() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = control_docks_.find(id);
        if (it != control_docks_.end()) {
            obs_frontend_remove_dock(id.c_str());
            delete it->second;
            control_docks_.erase(it);
        }
        active_states_.erase(id);
    };

    if (qApp && QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(qApp, run_cleanup, Qt::QueuedConnection);
    } else {
        run_cleanup();
    }
}

void DockManager::OpenControlDock(FilterState* state) {
    if (!state) return;
    std::string id = state->GetInstanceId();

    auto run_open = [this, state, id]() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = control_docks_.find(id);
        if (it != control_docks_.end() && it->second) {
            it->second->show();
            it->second->raise();
            it->second->activateWindow();
            return;
        }

        if (!main_window_) {
            main_window_ = reinterpret_cast<QMainWindow*>(obs_frontend_get_main_window());
        }

        auto* dock = new ControlDockWidget(state, main_window_);
        obs_frontend_add_custom_qdock(id.c_str(), dock);
        control_docks_[id] = dock;

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

void DockManager::OnFrontendEvent(enum obs_frontend_event event) {
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
        if (!main_window_) {
            Init(static_cast<QMainWindow*>(obs_frontend_get_main_window()));
        } else {
            AddMonitorToDocksMenu();
        }
    } else if (event == OBS_FRONTEND_EVENT_EXIT) {
        Shutdown();
    }
}

} // namespace AeriaSurround
