#include "global-surround.hpp"
#include <util/threading.h>
#include <util/platform.h>
#include <obs-module.h>
#include <cmath>
#include <algorithm>

namespace AeriaSurround {

struct audio_output_hook_t {
    struct audio_output_info info;
    size_t block_size;
    size_t channels;
    size_t planes;
    pthread_t thread;
    os_event_t *stop_event;
    bool initialized;
    audio_input_callback_t input_cb;
    void *input_param;
    pthread_mutex_t input_mutex;
};

GlobalSurround& GlobalSurround::Instance() {
    static GlobalSurround s_instance;
    return s_instance;
}

GlobalSurround::GlobalSurround() {
    audio_t* audio = obs_get_audio();
    if (audio) {
        sample_rate_ = audio_output_get_sample_rate(audio);
    }
    engine_.SetSampleRate(sample_rate_);
}

GlobalSurround::~GlobalSurround() {
    Shutdown();
}

void GlobalSurround::Init() {
    LoadConfig();
    HookAudioOutput();
}

void GlobalSurround::Shutdown() {
    SaveConfig();
    UnhookAudioOutput();
}

void GlobalSurround::SetEnabled(bool enabled) {
    enabled_.store(enabled);
    if (enabled && !hooked_.load()) {
        HookAudioOutput();
    }
    SaveConfig();
}

void GlobalSurround::HookAudioOutput() {
    if (hooked_.load()) return;

    audio_t* audio = obs_get_audio();
    if (!audio) return;

    auto* ao = reinterpret_cast<audio_output_hook_t*>(audio);

    bool found = false;
    if (ao->input_cb == ao->info.input_callback) {
        orig_input_cb_ = ao->input_cb;
        orig_input_param_ = ao->input_param;
        ao->input_cb = AudioInputCallback;
        ao->input_param = this;
        found = true;
    } else {
        uintptr_t target_cb = reinterpret_cast<uintptr_t>(ao->info.input_callback);
        uintptr_t* raw = reinterpret_cast<uintptr_t*>(audio);
        size_t start_idx = sizeof(struct audio_output_info) / sizeof(uintptr_t);
        for (size_t i = start_idx; i < 30; ++i) {
            if (raw[i] == target_cb) {
                orig_input_cb_ = reinterpret_cast<audio_input_callback_t>(raw[i]);
                orig_input_param_ = reinterpret_cast<void*>(raw[i + 1]);
                raw[i] = reinterpret_cast<uintptr_t>(AudioInputCallback);
                raw[i + 1] = reinterpret_cast<uintptr_t>(this);
                found = true;
                break;
            }
        }
    }

    if (found) {
        hooked_.store(true);
    }
}

void GlobalSurround::UnhookAudioOutput() {
    if (!hooked_.load()) return;

    audio_t* audio = obs_get_audio();
    if (audio && orig_input_cb_) {
        auto* ao = reinterpret_cast<audio_output_hook_t*>(audio);
        if (ao->input_cb == AudioInputCallback) {
            ao->input_cb = orig_input_cb_;
            ao->input_param = orig_input_param_;
        } else {
            uintptr_t my_cb = reinterpret_cast<uintptr_t>(AudioInputCallback);
            uintptr_t* raw = reinterpret_cast<uintptr_t*>(audio);
            size_t start_idx = sizeof(struct audio_output_info) / sizeof(uintptr_t);
            for (size_t i = start_idx; i < 30; ++i) {
                if (raw[i] == my_cb) {
                    raw[i] = reinterpret_cast<uintptr_t>(orig_input_cb_);
                    raw[i + 1] = reinterpret_cast<uintptr_t>(orig_input_param_);
                    break;
                }
            }
        }
    }

    hooked_.store(false);
    orig_input_cb_ = nullptr;
    orig_input_param_ = nullptr;
}

bool GlobalSurround::AudioInputCallback(void*, uint64_t prev_time, uint64_t audio_time,
                                        uint64_t* new_ts, uint32_t active_mixers,
                                        struct audio_output_data* mixes) {
    auto& self = GlobalSurround::Instance();
    if (!self.orig_input_cb_) return false;

    bool success = self.orig_input_cb_(self.orig_input_param_, prev_time, audio_time, new_ts, active_mixers, mixes);
    if (!success) return false;

    if (self.enabled_.load()) {
        self.ProcessMasterMix(mixes, active_mixers);
    }

    return true;
}

void GlobalSurround::ProcessMasterMix(struct audio_output_data* mixes, uint32_t active_mixers) {
    audio_t* audio = obs_get_audio();
    if (!audio) return;

    size_t channels = audio_output_get_channels(audio);
    if (channels < 6) {
        // Output not configured for surround (Stereo/Mono only in OBS Settings)
        return;
    }

    uint32_t sr = audio_output_get_sample_rate(audio);
    if (sr != sample_rate_) {
        sample_rate_ = sr;
        engine_.SetSampleRate(sr);
    }

    for (size_t mix_idx = 0; mix_idx < MAX_AUDIO_MIXES; ++mix_idx) {
        if (!(active_mixers & (1 << mix_idx))) continue;

        float** planes = mixes[mix_idx].data;
        if (!planes[0] || !planes[1]) continue;

        // Skip if this mix already has surround channel audio (5.1/7.1 file or already filtered)
        bool already_surround = false;
        for (size_t c = 2; c < channels; ++c) {
            if (!planes[c]) continue;
            for (size_t f = 0; f < AUDIO_OUTPUT_FRAMES; f += 8) {
                if (std::abs(planes[c][f]) > 1e-4f) {
                    already_surround = true;
                    break;
                }
            }
            if (already_surround) break;
        }

        if (already_surround) {
            continue;
        }

        // Check if stereo signal is present
        bool has_signal = false;
        for (size_t f = 0; f < AUDIO_OUTPUT_FRAMES; f += 8) {
            if (std::abs(planes[0][f]) > 1e-5f || std::abs(planes[1][f]) > 1e-5f) {
                has_signal = true;
                break;
            }
        }
        if (!has_signal) continue;

        // Process through Global Upmix Engine
        UpmixParams p = state_.GetParamsSnapshot();
        engine_.SetParams(p);
        engine_.Process(planes, 2, channels, AUDIO_OUTPUT_FRAMES);

        if (mix_idx == 0) {
            meter_.Process(planes, channels, AUDIO_OUTPUT_FRAMES);
        }
    }
}

void GlobalSurround::SaveConfig() {
    char* config_path = obs_module_config_path("global_surround.json");
    if (!config_path) return;

    obs_data_t* data = obs_data_create();
    state_.SaveSettings(data);
    obs_data_set_bool(data, "global_enabled", enabled_.load());

    obs_data_save_json_safe(data, config_path, "tmp", "bak");
    obs_data_release(data);
    bfree(config_path);
}

void GlobalSurround::LoadConfig() {
    char* config_path = obs_module_config_path("global_surround.json");
    if (!config_path) return;

    obs_data_t* data = obs_data_create_from_json_file_safe(config_path, "bak");
    if (data) {
        state_.LoadSettings(data);
        if (obs_data_has_user_value(data, "global_enabled")) {
            enabled_.store(obs_data_get_bool(data, "global_enabled"));
        }
        obs_data_release(data);
    }
    bfree(config_path);
}

} // namespace AeriaSurround
