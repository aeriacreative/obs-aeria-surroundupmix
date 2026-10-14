#pragma once

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace AeriaSurround {

constexpr size_t METER_MAX_CHANNELS = 8;
constexpr float MIN_DB = -60.0f;

inline float LinearToDb(float linear) {
    if (linear <= 0.000001f) return MIN_DB;
    float db = 20.0f * std::log10(linear);
    return (db < MIN_DB) ? MIN_DB : (db > 12.0f ? 12.0f : db);
}

inline float DbToLinear(float db) {
    if (db <= MIN_DB) return 0.0f;
    return std::pow(10.0f, db / 20.0f);
}

struct ChannelMeterData {
    float peak_linear = 0.0f;
    float peak_db = MIN_DB;
    float rms_linear = 0.0f;
    float rms_db = MIN_DB;
    bool  clip = false;
};

class LevelMeter {
public:
    LevelMeter();

    // Audio thread
    void Process(const float* const* channel_data, size_t channels, uint32_t frames);
    void Reset();

    // UI thread
    void GetSnapshot(ChannelMeterData out[METER_MAX_CHANNELS], size_t count);

private:
    std::atomic<float> peak_levels_[METER_MAX_CHANNELS];
    std::atomic<float> rms_levels_[METER_MAX_CHANNELS];
    std::atomic<bool>  clip_flags_[METER_MAX_CHANNELS];

    float peak_hold_[METER_MAX_CHANNELS];
    int   hold_countdown_[METER_MAX_CHANNELS];
};

} // namespace AeriaSurround
