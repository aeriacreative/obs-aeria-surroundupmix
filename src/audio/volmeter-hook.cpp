#include "volmeter-hook.hpp"
#include "../plugin-macros.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <obs.h>
#include <media-io/audio-io.h>
#include <algorithm>
#include <cstring>

namespace AeriaSurround {

static void* s_target_func = nullptr;
static uint8_t s_original_bytes[14] = {0};
static bool s_hooked = false;

// Replacement for obs_volmeter_get_nr_channels
static int Hooked_obs_volmeter_get_nr_channels(obs_volmeter_t* volmeter) {
    if (!volmeter) return 0;

    // volmeter->source is located at offset 8 on x64
    obs_source_t* source = *reinterpret_cast<obs_source_t**>(reinterpret_cast<char*>(volmeter) + 8);
    if (source) {
        bool has_surround_filter = false;
        obs_source_enum_filters(source, [](obs_source_t*, obs_source_t* filter, void* param) {
            bool* found = static_cast<bool*>(param);
            const char* id = obs_source_get_id(filter);
            if (id && std::strcmp(id, "aeria_surround_upmix_filter") == 0 && obs_source_enabled(filter)) {
                *found = true;
            }
        }, &has_surround_filter);

        if (has_surround_filter) {
            audio_t* audio = obs_get_audio();
            if (audio) {
                size_t ch = audio_output_get_channels(audio);
                if (ch >= 6) {
                    return static_cast<int>(ch);
                }
            }
        }

        // Standard behavior for other sources
        int src_channels = get_audio_channels(obs_source_get_speaker_layout(source));
        struct obs_audio_info audio_info;
        int obs_channels = obs_get_audio_info(&audio_info) ? get_audio_channels(audio_info.speakers) : 2;
        return std::clamp(src_channels, 0, obs_channels);
    }

    struct obs_audio_info audio_info;
    int obs_channels = obs_get_audio_info(&audio_info) ? get_audio_channels(audio_info.speakers) : 2;
    return std::clamp(0, 0, obs_channels);
}

void InstallVolmeterHook() {
    if (s_hooked) return;

    HMODULE hObs = GetModuleHandleA("obs.dll");
    if (!hObs) {
        hObs = GetModuleHandleA("libobs.dll");
    }
    if (!hObs) return;

    s_target_func = reinterpret_cast<void*>(GetProcAddress(hObs, "obs_volmeter_get_nr_channels"));
    if (!s_target_func) {
        blog(LOG_WARNING, "Failed to locate obs_volmeter_get_nr_channels for mixer multi-channel hook.");
        return;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(s_target_func, 14, PAGE_EXECUTE_READWRITE, &old_protect)) {
        blog(LOG_WARNING, "VirtualProtect failed on obs_volmeter_get_nr_channels.");
        return;
    }

    // Save original 14 bytes
    std::memcpy(s_original_bytes, s_target_func, 14);

    // 64-bit absolute jump: FF 25 00 00 00 00 [64-bit address]
    uint8_t jmp[14];
    jmp[0] = 0xFF;
    jmp[1] = 0x25;
    *reinterpret_cast<uint32_t*>(&jmp[2]) = 0; // RIP relative displacement 0
    *reinterpret_cast<void**>(&jmp[6]) = reinterpret_cast<void*>(&Hooked_obs_volmeter_get_nr_channels);

    std::memcpy(s_target_func, jmp, 14);
    VirtualProtect(s_target_func, 14, old_protect, &old_protect);
    FlushInstructionCache(GetCurrentProcess(), s_target_func, 14);

    s_hooked = true;
    blog(LOG_INFO, "Installed Audio Mixer 6/8-channel volmeter hook successfully.");
}

void RemoveVolmeterHook() {
    if (!s_hooked || !s_target_func) return;

    DWORD old_protect = 0;
    if (VirtualProtect(s_target_func, 14, PAGE_EXECUTE_READWRITE, &old_protect)) {
        std::memcpy(s_target_func, s_original_bytes, 14);
        VirtualProtect(s_target_func, 14, old_protect, &old_protect);
        FlushInstructionCache(GetCurrentProcess(), s_target_func, 14);
    }

    s_hooked = false;
    s_target_func = nullptr;
    blog(LOG_INFO, "Removed Audio Mixer volmeter hook.");
}

} // namespace AeriaSurround
