#pragma once

#include <string>
#include <map>
#include <mutex>
#include <memory>
#include <atomic>
#include <obs-frontend-api.h>
#include "control-dock.hpp"
#include "monitor-dock.hpp"
#include "../state/filter-state.hpp"

class QMainWindow;

namespace AeriaSurround {

class DockManager {
public:
    static DockManager& Instance();

    void Init(QMainWindow* main_window);
    void Shutdown();

    // Filter registry
    void RegisterFilterState(std::shared_ptr<FilterState> state);
    void UnregisterFilterState(std::shared_ptr<FilterState> state);

    // Dock control
    void OpenControlDock(std::shared_ptr<FilterState> state);
    void ShowMonitorDock();
    bool IsReady() const { return main_window_ != nullptr; }
    void RefreshAudioMonitoring();

    void OnFrontendEvent(enum obs_frontend_event event);

private:
    DockManager() = default;
    ~DockManager() = default;

    std::mutex  mutex_;
    std::atomic<bool> shutting_down_{false}; ///< Set in Shutdown() to block late callbacks
    QMainWindow* main_window_ = nullptr;
    MonitorDockWidget* monitor_dock_  = nullptr;
    ControlDockWidget* control_dock_  = nullptr; ///< Single unified Surround Control dock

    std::map<std::string, std::shared_ptr<FilterState>> active_states_;

    void AddMonitorToDocksMenu();
    void AddControlDockToDocksMenu(); ///< Add Sorround Control toggle to Docks menu
};

} // namespace AeriaSurround
