#include "filter-state.hpp"
#include <sstream>
#include <iomanip>

namespace AeriaSurround {

static const char* S_MODE = "mode";
static const char* S_GAIN_L = "gain_l";
static const char* S_GAIN_R = "gain_r";
static const char* S_GAIN_C = "gain_c";
static const char* S_GAIN_LFE = "gain_lfe";
static const char* S_GAIN_SL = "gain_sl";
static const char* S_GAIN_SR = "gain_sr";
static const char* S_GAIN_RL = "gain_rl";
static const char* S_GAIN_RR = "gain_rr";
static const char* S_PAN_X = "pan_x";
static const char* S_PAN_Y = "pan_y";
static const char* S_CENTER_SPREAD = "center_spread";
static const char* S_SURROUND_SPREAD = "surround_spread";
static const char* S_LFE_CUTOFF = "lfe_cutoff";

FilterState::FilterState(obs_source_t* filter_context)
    : filter_context_(filter_context)
{
    std::ostringstream ss;
    ss << "surround_control_" << std::hex << reinterpret_cast<uintptr_t>(this);
    instance_id_ = ss.str();
}

FilterState::~FilterState() {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    on_change_cb_ = nullptr;
}

std::string FilterState::GetDisplayName() const {
    if (!filter_context_) return "Sorround Upmix";
    const char* filter_name = obs_source_get_name(filter_context_);
    obs_source_t* parent = obs_filter_get_parent(filter_context_);
    const char* parent_name = parent ? obs_source_get_name(parent) : nullptr;

    std::string result;
    if (parent_name && *parent_name) {
        result = std::string(parent_name) + " — " + (filter_name ? filter_name : "Sorround Upmix");
    } else {
        result = filter_name ? filter_name : "Sorround Upmix";
    }
    return result;
}

void FilterState::LoadSettings(obs_data_t* settings) {
    if (!settings) return;

    UpmixParams p;
    {
        std::lock_guard<std::mutex> lock(param_mutex_);
        p = params_;
    }

    p.mode = static_cast<UpmixMode>(obs_data_get_int(settings, S_MODE));
    p.channel_gain_db[0] = static_cast<float>(obs_data_get_double(settings, S_GAIN_L));
    p.channel_gain_db[1] = static_cast<float>(obs_data_get_double(settings, S_GAIN_R));
    p.channel_gain_db[2] = static_cast<float>(obs_data_get_double(settings, S_GAIN_C));
    p.channel_gain_db[3] = static_cast<float>(obs_data_get_double(settings, S_GAIN_LFE));
    p.channel_gain_db[4] = static_cast<float>(obs_data_get_double(settings, S_GAIN_RL));
    p.channel_gain_db[5] = static_cast<float>(obs_data_get_double(settings, S_GAIN_RR));
    p.channel_gain_db[6] = static_cast<float>(obs_data_get_double(settings, S_GAIN_SL));
    p.channel_gain_db[7] = static_cast<float>(obs_data_get_double(settings, S_GAIN_SR));

    p.pan_x = static_cast<float>(obs_data_get_double(settings, S_PAN_X));
    p.pan_y = static_cast<float>(obs_data_get_double(settings, S_PAN_Y));
    p.center_spread = static_cast<float>(obs_data_get_double(settings, S_CENTER_SPREAD));
    p.surround_spread = static_cast<float>(obs_data_get_double(settings, S_SURROUND_SPREAD));

    double cutoff = obs_data_get_double(settings, S_LFE_CUTOFF);
    p.lfe_cutoff_hz = (cutoff > 10.0) ? static_cast<float>(cutoff) : 120.0f;

    {
        std::lock_guard<std::mutex> lock(param_mutex_);
        params_ = p;
    }

    NotifyChange();
}

void FilterState::SaveSettings(obs_data_t* settings) {
    if (!settings) return;

    UpmixParams p;
    {
        std::lock_guard<std::mutex> lock(param_mutex_);
        p = params_;
    }

    obs_data_set_int(settings, S_MODE, static_cast<long long>(p.mode));
    obs_data_set_double(settings, S_GAIN_L, p.channel_gain_db[0]);
    obs_data_set_double(settings, S_GAIN_R, p.channel_gain_db[1]);
    obs_data_set_double(settings, S_GAIN_C, p.channel_gain_db[2]);
    obs_data_set_double(settings, S_GAIN_LFE, p.channel_gain_db[3]);
    obs_data_set_double(settings, S_GAIN_RL, p.channel_gain_db[4]);
    obs_data_set_double(settings, S_GAIN_RR, p.channel_gain_db[5]);
    obs_data_set_double(settings, S_GAIN_SL, p.channel_gain_db[6]);
    obs_data_set_double(settings, S_GAIN_SR, p.channel_gain_db[7]);

    obs_data_set_double(settings, S_PAN_X, p.pan_x);
    obs_data_set_double(settings, S_PAN_Y, p.pan_y);
    obs_data_set_double(settings, S_CENTER_SPREAD, p.center_spread);
    obs_data_set_double(settings, S_SURROUND_SPREAD, p.surround_spread);
    obs_data_set_double(settings, S_LFE_CUTOFF, p.lfe_cutoff_hz);
}

UpmixParams FilterState::GetParamsSnapshot() {
    std::lock_guard<std::mutex> lock(param_mutex_);
    return params_;
}

UpmixParams FilterState::GetParams() {
    std::lock_guard<std::mutex> lock(param_mutex_);
    return params_;
}

void FilterState::SetChannelGain(ChannelIndex ch, float db) {
    size_t idx = static_cast<size_t>(ch);
    if (idx >= 8) return;

    {
        std::lock_guard<std::mutex> lock(param_mutex_);
        params_.channel_gain_db[idx] = db;
    }
    SyncToObsSettings();
    NotifyChange();
}

void FilterState::SetPanning(float x, float y) {
    {
        std::lock_guard<std::mutex> lock(param_mutex_);
        params_.pan_x = x;
        params_.pan_y = y;
    }
    SyncToObsSettings();
    NotifyChange();
}

void FilterState::SetMode(UpmixMode mode) {
    {
        std::lock_guard<std::mutex> lock(param_mutex_);
        params_.mode = mode;
    }
    SyncToObsSettings();
    NotifyChange();
}

void FilterState::SetSpread(float center, float surround) {
    {
        std::lock_guard<std::mutex> lock(param_mutex_);
        params_.center_spread = center;
        params_.surround_spread = surround;
    }
    SyncToObsSettings();
    NotifyChange();
}

void FilterState::SetLfeCutoff(float cutoff_hz) {
    {
        std::lock_guard<std::mutex> lock(param_mutex_);
        params_.lfe_cutoff_hz = cutoff_hz;
    }
    SyncToObsSettings();
    NotifyChange();
}

void FilterState::SetSoloChannel(int ch) {
    std::lock_guard<std::mutex> lock(param_mutex_);
    params_.solo_channel = ch;
}

int FilterState::GetSoloChannel() const {
    std::lock_guard<std::mutex> lock(param_mutex_);
    return params_.solo_channel;
}

void FilterState::SetParamsFromUI(const UpmixParams& params, bool sync_obs_settings) {
    {
        std::lock_guard<std::mutex> lock(param_mutex_);
        params_ = params;
    }
    if (sync_obs_settings) {
        SyncToObsSettings();
    }
    NotifyChange();
}

void FilterState::SetChangeCallback(ChangeCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    on_change_cb_ = cb;
}

void FilterState::NotifyChange() {
    ChangeCallback cb;
    {
        std::lock_guard<std::mutex> lock(cb_mutex_);
        cb = on_change_cb_;
    }
    if (cb) {
        UpmixParams p = GetParams();
        cb(p);
    }
}

void FilterState::SyncToObsSettings() {
    if (!filter_context_) return;
    obs_data_t* settings = obs_source_get_settings(filter_context_);
    if (!settings) return;
    SaveSettings(settings);
    obs_source_update(filter_context_, settings);
    obs_data_release(settings);
}

} // namespace AeriaSurround
