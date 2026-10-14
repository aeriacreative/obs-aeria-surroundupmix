#pragma once

#include "channel-layout.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace AeriaSurround {

struct UpmixParams {
    UpmixMode mode = UpmixMode::Auto;
    float channel_gain_db[8] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float pan_x = 0.0f;           // [-1.0, 1.0]
    float pan_y = 0.0f;           // [-1.0, 1.0]
    float center_spread = 0.5f;   // [0.0, 1.0]
    float surround_spread = 0.7f; // [0.0, 1.0]
    float lfe_cutoff_hz = 120.0f; // [60.0, 250.0]
    int   solo_channel = -1;      // -1 = all active, 0..7 = solo preview channel
    bool  enabled = true;
};

class BiquadFilter {
public:
    BiquadFilter();
    void SetLowPass(float sample_rate, float cutoff_hz, float q = 0.7071f);
    float ProcessSample(float in);
    void Reset();

private:
    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;
    float a1_ = 0.0f, a2_ = 0.0f;
    float z1_ = 0.0f, z2_ = 0.0f;
};

class UpmixEngine {
public:
    UpmixEngine();

    void SetSampleRate(uint32_t sample_rate);
    void SetParams(const UpmixParams& params);
    const UpmixParams& GetParams() const { return params_; }

    // In-place or multi-plane processing
    // input_channels: channels provided in audio->data
    // output_channels: target channels of OBS output (typically 2, 6, or 8)
    void Process(float* const* channel_data, size_t input_channels, size_t output_channels, uint32_t frames);

    void Reset();

private:
    UpmixParams params_;
    uint32_t sample_rate_ = 48000;

    BiquadFilter lfe_filter_;

    // Delay line for surround decorrelation (max 100ms = 4800 samples)
    std::vector<float> delay_buffer_;
    size_t delay_write_pos_ = 0;
    size_t delay_samples_ = 480; // 10ms at 48kHz

    // Scratch buffer for processing channels when output planes need allocation
    std::vector<float> scratch_[8];
};

} // namespace AeriaSurround
