#include "upmix-engine.hpp"
#include "level-meter.hpp"
#include <algorithm>
#include <cmath>

namespace AeriaSurround {

static constexpr float PI = 3.14159265358979323846f;
static constexpr float SQRT2 = 1.4142135623730950488f;

BiquadFilter::BiquadFilter() {
    Reset();
}

void BiquadFilter::Reset() {
    z1_ = 0.0f;
    z2_ = 0.0f;
}

void BiquadFilter::SetLowPass(float sample_rate, float cutoff_hz, float q) {
    if (sample_rate <= 0.0f) sample_rate = 48000.0f;
    cutoff_hz = std::clamp(cutoff_hz, 20.0f, sample_rate * 0.45f);

    float w0 = 2.0f * PI * cutoff_hz / sample_rate;
    float cos_w = std::cos(w0);
    float sin_w = std::sin(w0);
    float alpha = sin_w / (2.0f * q);

    float a0 = 1.0f + alpha;
    b0_ = ((1.0f - cos_w) * 0.5f) / a0;
    b1_ = (1.0f - cos_w) / a0;
    b2_ = ((1.0f - cos_w) * 0.5f) / a0;
    a1_ = (-2.0f * cos_w) / a0;
    a2_ = (1.0f - alpha) / a0;
}

float BiquadFilter::ProcessSample(float in) {
    float out = b0_ * in + z1_;
    z1_ = b1_ * in - a1_ * out + z2_;
    z2_ = b2_ * in - a2_ * out;
    return out;
}

UpmixEngine::UpmixEngine() {
    SetSampleRate(48000);
}

void UpmixEngine::SetSampleRate(uint32_t sample_rate) {
    sample_rate_ = (sample_rate > 0) ? sample_rate : 48000;
    lfe_filter_.SetLowPass(static_cast<float>(sample_rate_), params_.lfe_cutoff_hz);

    // 15ms delay line
    delay_samples_ = static_cast<size_t>(sample_rate_ * 0.015f);
    if (delay_samples_ < 10) delay_samples_ = 10;
    delay_buffer_.assign(delay_samples_ * 2, 0.0f);
    delay_write_pos_ = 0;
}

void UpmixEngine::SetParams(const UpmixParams& params) {
    bool lfe_changed = (params.lfe_cutoff_hz != params_.lfe_cutoff_hz);
    params_ = params;
    if (lfe_changed) {
        lfe_filter_.SetLowPass(static_cast<float>(sample_rate_), params_.lfe_cutoff_hz);
    }
}

void UpmixEngine::Reset() {
    lfe_filter_.Reset();
    std::fill(delay_buffer_.begin(), delay_buffer_.end(), 0.0f);
    delay_write_pos_ = 0;
}

void UpmixEngine::Process(float* const* channel_data, size_t input_channels, size_t output_channels, uint32_t frames) {
    if (!params_.enabled || !channel_data || frames == 0 || input_channels == 0) return;

    // Precompute linear gains for all 8 channels
    float mul[8];
    for (size_t i = 0; i < 8; ++i) {
        mul[i] = DbToLinear(params_.channel_gain_db[i]);
    }

    // Panning multipliers
    float pan_x = std::clamp(params_.pan_x, -1.0f, 1.0f);
    float pan_y = std::clamp(params_.pan_y, -1.0f, 1.0f);

    float pan_x_ang = (pan_x + 1.0f) * 0.25f * PI;
    float pan_l = SQRT2 * std::cos(pan_x_ang);
    float pan_r = SQRT2 * std::sin(pan_x_ang);

    // pan_y: +1 = front only, 0 = equal, -1 = rear only
    float pan_y_ang = (pan_y + 1.0f) * 0.25f * PI;
    float pan_rear  = SQRT2 * std::cos(pan_y_ang);
    float pan_front = SQRT2 * std::sin(pan_y_ang);

    // Direct mode: simply apply channel gains to existing channels
    if (params_.mode == UpmixMode::Direct) {
        size_t n = std::min(input_channels, size_t(8));
        for (size_t c = 0; c < n; ++c) {
            float* ch = channel_data[c];
            if (!ch) continue;
            float g = mul[c];
            for (uint32_t f = 0; f < frames; ++f) {
                ch[f] *= g;
            }
        }
        return;
    }

    // Check if input is already multichannel (e.g., C or LFE or Surrounds already present and active)
    bool already_multichannel = false;
    if (input_channels >= 6 && channel_data[2] && channel_data[3]) {
        float max_surr = 0.0f;
        for (uint32_t f = 0; f < std::min(frames, 64u); ++f) {
            max_surr = std::max(max_surr, std::abs(channel_data[2][f]));
            max_surr = std::max(max_surr, std::abs(channel_data[3][f]));
        }
        if (max_surr > 0.001f) {
            already_multichannel = true;
        }
    }

    if (already_multichannel) {
        // Apply gains and panning without re-upmixing
        size_t n = std::min(input_channels, size_t(8));
        for (size_t c = 0; c < n; ++c) {
            float* ch = channel_data[c];
            if (!ch) continue;
            float g = mul[c];
            if (c == 0 || c == 4 || c == 6) g *= pan_l;
            if (c == 1 || c == 5 || c == 7) g *= pan_r;
            if (c <= 2) g *= pan_front;
            else if (c >= 4) g *= pan_rear;

            for (uint32_t f = 0; f < frames; ++f) {
                ch[f] *= g;
            }
        }
        return;
    }

    // Upmix Mono / Stereo to Surround
    const float* in_l = channel_data[0];
    const float* in_r = (input_channels >= 2 && channel_data[1]) ? channel_data[1] : in_l;
    if (!in_l) return;

    // Ensure scratch buffers are sized for frames
    for (size_t i = 0; i < 8; ++i) {
        if (scratch_[i].size() < frames) {
            scratch_[i].resize(frames);
        }
    }

    float* out_fl  = scratch_[0].data();
    float* out_fr  = scratch_[1].data();
    float* out_c   = scratch_[2].data();
    float* out_lfe = scratch_[3].data();
    float* out_rl  = scratch_[4].data();
    float* out_rr  = scratch_[5].data();
    float* out_sl  = scratch_[6].data();
    float* out_sr  = scratch_[7].data();

    float center_spread   = std::clamp(params_.center_spread, 0.0f, 1.0f);
    float surround_spread = std::clamp(params_.surround_spread, 0.0f, 1.0f);

    size_t delay_cap = delay_buffer_.size();
    if (delay_cap == 0) {
        delay_cap = 1;
        delay_buffer_.resize(1, 0.0f);
    }

    for (uint32_t f = 0; f < frames; ++f) {
        float l = in_l[f];
        float r = in_r ? in_r[f] : l;

        // Mid / Side decomposition
        float mid  = 0.5f * (l + r);
        float side = 0.5f * (l - r) * surround_spread;

        // Center extraction
        float c_sig = mid * mul[2] * pan_front;
        out_c[f] = c_sig;

        // Front Left / Right with Center subtraction
        float fl_sig = (l - 0.5f * center_spread * mid) * mul[0] * pan_l * pan_front;
        float fr_sig = (r - 0.5f * center_spread * mid) * mul[1] * pan_r * pan_front;
        out_fl[f] = fl_sig;
        out_fr[f] = fr_sig;

        // Subwoofer / LFE via 2nd-order resonant low-pass filter
        float lfe_in = mid;
        float lfe_sig = lfe_filter_.ProcessSample(lfe_in) * mul[3];
        out_lfe[f] = lfe_sig;

        // Surround ambience with decorrelation delay
        float delayed_side = delay_buffer_[delay_write_pos_];
        delay_buffer_[delay_write_pos_] = side;
        delay_write_pos_ = (delay_write_pos_ + 1) % delay_cap;

        // Side Left & Side Right (SL, SR)
        float sl_sig = delayed_side * mul[6] * pan_l * pan_rear;
        float sr_sig = -delayed_side * mul[7] * pan_r * pan_rear;
        out_sl[f] = sl_sig;
        out_sr[f] = sr_sig;

        // Rear Left & Rear Right (RL, RR)
        float rl_sig = (delayed_side * 0.7071f - 0.1f * l) * mul[4] * pan_l * pan_rear;
        float rr_sig = (-delayed_side * 0.7071f - 0.1f * r) * mul[5] * pan_r * pan_rear;
        out_rl[f] = rl_sig;
        out_rr[f] = rr_sig;
    }

    // Write back into channel_data planes
    // If output is stereo (only 2 planes available in OBS)
    if (output_channels <= 2) {
        if (channel_data[0]) {
            std::copy(out_fl, out_fl + frames, channel_data[0]);
        }
        if (channel_data[1]) {
            std::copy(out_fr, out_fr + frames, channel_data[1]);
        }
        return;
    }

    // Multichannel output: 5.1 (6 planes: FL, FR, FC, LFE, RL, RR)
    if (output_channels >= 6) {
        if (channel_data[0]) std::copy(out_fl, out_fl + frames, channel_data[0]);
        if (channel_data[1]) std::copy(out_fr, out_fr + frames, channel_data[1]);
        if (channel_data[2]) std::copy(out_c,  out_c + frames,  channel_data[2]);
        if (channel_data[3]) std::copy(out_lfe, out_lfe + frames, channel_data[3]);
        if (channel_data[4]) std::copy(out_rl, out_rl + frames, channel_data[4]);
        if (channel_data[5]) std::copy(out_rr, out_rr + frames, channel_data[5]);
    }

    // 7.1 (8 planes: adds SL, SR)
    if (output_channels >= 8) {
        if (channel_data[6]) std::copy(out_sl, out_sl + frames, channel_data[6]);
        if (channel_data[7]) std::copy(out_sr, out_sr + frames, channel_data[7]);
    }

    // Solo Channel Preview Mode: mute all channels except the soloed channel
    if (params_.solo_channel >= 0) {
        size_t solo = static_cast<size_t>(params_.solo_channel);
        for (size_t c = 0; c < output_channels; ++c) {
            if (c != solo && channel_data[c]) {
                std::fill(channel_data[c], channel_data[c] + frames, 0.0f);
            }
        }
    }
}

} // namespace AeriaSurround
