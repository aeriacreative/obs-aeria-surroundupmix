#pragma once

#include "../audio/channel-layout.hpp"
#include "../audio/level-meter.hpp"
#include "../audio/upmix-engine.hpp"
#include <mutex>
#include <string>
#include <functional>
#include <obs.h>

namespace AeriaSurround {

class FilterState {
public:
    explicit FilterState(obs_source_t* filter_context);
    ~FilterState();

    const std::string& GetInstanceId() const { return instance_id_; }
    std::string GetDisplayName() const;
    obs_source_t* GetFilterContext() const { return filter_context_; }

    // Settings serialization
    void LoadSettings(obs_data_t* settings);
    void SaveSettings(obs_data_t* settings);

    // Audio DSP thread
    UpmixParams GetParamsSnapshot();
    LevelMeter& GetMeter() { return meter_; }

    // UI thread access
    UpmixParams GetParams();
    void SetChannelGain(ChannelIndex ch, float db);
    void SetPanning(float x, float y);
    void SetMode(UpmixMode mode);
    void SetSpread(float center, float surround);
    void SetLfeCutoff(float cutoff_hz);
    void SetSoloChannel(int ch);
    int GetSoloChannel() const;

    void SetParamsFromUI(const UpmixParams& params, bool sync_obs_settings = true);

    // Callback when UI or properties change
    using ChangeCallback = std::function<void(const UpmixParams&)>;
    void SetChangeCallback(ChangeCallback cb);

private:
    std::string instance_id_;
    obs_source_t* filter_context_ = nullptr;

    mutable std::mutex param_mutex_;
    UpmixParams params_;
    LevelMeter  meter_;

    std::mutex cb_mutex_;
    ChangeCallback on_change_cb_;

    void NotifyChange();
    void SyncToObsSettings();
};

} // namespace AeriaSurround
