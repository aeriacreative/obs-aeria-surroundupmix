#pragma once

#include <string>
#include <map>
#include <mutex>
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
    void RegisterFilterState(FilterState* state);
    void UnregisterFilterState(FilterState* state);

    // Dock control
    void OpenControlDock(FilterState* state);
    void ShowMonitorDock();
    bool IsReady() const { return main_window_ != nullptr; }
    void RefreshAudioMonitoring();

    void OnFrontendEvent(enum obs_frontend_event event);

private:
    DockManager() = default;
    ~DockManager() = default;

    std::mutex mutex_;
    QMainWindow* main_window_ = nullptr;
    MonitorDockWidget* monitor_dock_ = nullptr;

    std::map<std::string, FilterState*> active_states_;
    ControlDockWidget* control_dock_ = nullptr; ///< Single unified Sorround Control dock

    void AddMonitorToDocksMenu();
};

} // namespace AeriaSurround
