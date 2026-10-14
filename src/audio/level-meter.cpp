#include "level-meter.hpp"
#include <algorithm>
#include <cmath>

namespace AeriaSurround {

LevelMeter::LevelMeter() {
    Reset();
}

void LevelMeter::Reset() {
    for (size_t i = 0; i < METER_MAX_CHANNELS; ++i) {
        peak_levels_[i].store(0.0f, std::memory_order_relaxed);
        rms_levels_[i].store(0.0f, std::memory_order_relaxed);
        clip_flags_[i].store(false, std::memory_order_relaxed);
        peak_hold_[i] = 0.0f;
        hold_countdown_[i] = 0;
    }
}

void LevelMeter::Process(const float* const* channel_data, size_t channels, uint32_t frames) {
    if (!channel_data || frames == 0) return;

    size_t active_channels = std::min(channels, METER_MAX_CHANNELS);
    for (size_t c = 0; c < active_channels; ++c) {
        const float* samples = channel_data[c];
        if (!samples) {
            peak_levels_[c].store(0.0f, std::memory_order_relaxed);
            rms_levels_[c].store(0.0f, std::memory_order_relaxed);
            continue;
        }

        float max_sample = 0.0f;
        double sum_squares = 0.0;

        for (uint32_t f = 0; f < frames; ++f) {
            float s = std::abs(samples[f]);
            if (s > max_sample) max_sample = s;
            sum_squares += static_cast<double>(s) * static_cast<double>(s);
        }

        float rms = static_cast<float>(std::sqrt(sum_squares / static_cast<double>(frames)));

        if (max_sample >= 1.0f) {
            clip_flags_[c].store(true, std::memory_order_relaxed);
        }

        // Store peak with fast attack, smooth decay
        float prev_peak = peak_levels_[c].load(std::memory_order_relaxed);
        if (max_sample >= prev_peak) {
            peak_levels_[c].store(max_sample, std::memory_order_relaxed);
        } else {
            // Decay by ~0.95
            peak_levels_[c].store(prev_peak * 0.95f, std::memory_order_relaxed);
        }

        rms_levels_[c].store(rms, std::memory_order_relaxed);
    }

    // Zero out channels above active_channels
    for (size_t c = active_channels; c < METER_MAX_CHANNELS; ++c) {
        peak_levels_[c].store(0.0f, std::memory_order_relaxed);
        rms_levels_[c].store(0.0f, std::memory_order_relaxed);
    }
}

void LevelMeter::GetSnapshot(ChannelMeterData out[METER_MAX_CHANNELS], size_t count) {
    size_t n = std::min(count, METER_MAX_CHANNELS);
    for (size_t c = 0; c < n; ++c) {
        float peak = peak_levels_[c].load(std::memory_order_relaxed);
        float rms  = rms_levels_[c].load(std::memory_order_relaxed);
        bool  clip = clip_flags_[c].exchange(false, std::memory_order_relaxed);

        // Peak hold decay
        if (peak >= peak_hold_[c]) {
            peak_hold_[c] = peak;
            hold_countdown_[c] = 20; // ~600ms hold at 30fps
        } else {
            if (hold_countdown_[c] > 0) {
                --hold_countdown_[c];
            } else {
                peak_hold_[c] = std::max(0.0f, peak_hold_[c] - 0.02f);
            }
        }

        out[c].peak_linear = peak;
        out[c].peak_db     = LinearToDb(peak);
        out[c].rms_linear  = rms;
        out[c].rms_db      = LinearToDb(rms);
        out[c].clip        = clip;
    }
}

} // namespace AeriaSurround
