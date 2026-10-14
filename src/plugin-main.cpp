#include "plugin-main.hpp"
#include "plugin-macros.h"
#include "audio/surround-filter.hpp"
#include "audio/global-surround.hpp"
#include "audio/volmeter-hook.hpp"
#include "frontend/dock-manager.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")
OBS_MODULE_AUTHOR(PLUGIN_AUTHOR)

namespace {
    void FrontendEventCallback(enum obs_frontend_event event, void*) {
        AeriaSurround::DockManager::Instance().OnFrontendEvent(event);
    }
}

bool obs_module_load(void) {
    blog(LOG_INFO, "Loading %s v%s by %s", PLUGIN_NAME, PLUGIN_VERSION, PLUGIN_AUTHOR);
    blog(LOG_INFO, "%s", PLUGIN_CREDITS);

    // Register audio filter
    obs_register_source(&surround_filter_info);

    // Install inline hook so OBS Audio Mixer displays 6 or 8 bars for surround upmix sources
    AeriaSurround::InstallVolmeterHook();

    // Initialize global surround master output engine
    AeriaSurround::GlobalSurround::Instance().Init();

    // Register frontend event callback
    obs_frontend_add_event_callback(FrontendEventCallback, nullptr);

    // Initialize docks if frontend API is already available
    QMainWindow* main_window = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (main_window) {
        AeriaSurround::DockManager::Instance().Init(main_window);
    }

    obs_frontend_add_tools_menu_item("Sorround Monitor", [](void*) {
        AeriaSurround::DockManager::Instance().ShowMonitorDock();
    }, nullptr);

    blog(LOG_INFO, "Plugin %s loaded successfully.", PLUGIN_NAME);
    return true;
}

void obs_module_unload(void) {
    blog(LOG_INFO, "Unloading %s", PLUGIN_NAME);
    obs_frontend_remove_event_callback(FrontendEventCallback, nullptr);
    AeriaSurround::GlobalSurround::Instance().Shutdown();
    AeriaSurround::RemoveVolmeterHook();
    AeriaSurround::DockManager::Instance().Shutdown();
}

MODULE_EXPORT const char* obs_module_description(void) {
    return "Aeria Sorround Upmix — 5.1/7.1 Audio Upmixing, Channel Gain Control, and Surround Monitoring Plugin";
}

MODULE_EXPORT const char* obs_module_name(void) {
    return PLUGIN_NAME;
}
