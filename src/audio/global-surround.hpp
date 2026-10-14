#pragma once

#include <obs.h>
#include <media-io/audio-io.h>
#include <atomic>
#include <mutex>
#include "upmix-engine.hpp"
#include "../state/filter-state.hpp"
#include "level-meter.hpp"

namespace AeriaSurround {

class GlobalSurround {
public:
    static GlobalSurround& Instance();

    void Init();
    void Shutdown();

    bool IsEnabled() const { return enabled_.load(); }
    void SetEnabled(bool enabled);

    FilterState& GetState() { return state_; }
    LevelMeter&  GetMeter() { return meter_; }

    void ProcessMasterMix(struct audio_output_data* mixes, uint32_t active_mixers);

    void SaveConfig();
    void LoadConfig();

private:
    GlobalSurround();
    ~GlobalSurround();

    std::atomic<bool> enabled_{false};
    std::atomic<bool> hooked_{false};

    FilterState state_{nullptr};
    UpmixEngine engine_;
    LevelMeter  meter_;

    uint32_t sample_rate_ = 48000;

    audio_input_callback_t orig_input_cb_ = nullptr;
    void* orig_input_param_ = nullptr;

    void HookAudioOutput();
    void UnhookAudioOutput();

    static bool AudioInputCallback(void* param, uint64_t prev_time, uint64_t audio_time,
                                   uint64_t* new_ts, uint32_t active_mixers,
                                   struct audio_output_data* mixes);
};

} // namespace AeriaSurround
