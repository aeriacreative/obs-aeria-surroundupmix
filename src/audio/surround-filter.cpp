#include "surround-filter.hpp"
#include "upmix-engine.hpp"
#include "../state/filter-state.hpp"
#include "../frontend/dock-manager.hpp"
#include "../plugin-macros.h"
#include <media-io/audio-io.h>
#include <obs-module.h>
#include <algorithm>

using namespace AeriaSurround;

struct SurroundFilterContext {
    obs_source_t* context = nullptr;
    FilterState*  state   = nullptr;
    UpmixEngine   engine;
    uint32_t      sample_rate = 48000;
};

static const char* filter_get_name(void*) {
    return obs_module_text("Filter.Name");
}

static void filter_destroy(void* data) {
    auto* ctx = static_cast<SurroundFilterContext*>(data);
    if (!ctx) return;

    if (ctx->state) {
        DockManager::Instance().UnregisterFilterState(ctx->state);
        delete ctx->state;
        ctx->state = nullptr;
    }

    delete ctx;
}

static void filter_update(void* data, obs_data_t* settings) {
    auto* ctx = static_cast<SurroundFilterContext*>(data);
    if (!ctx || !ctx->state) return;

    ctx->state->LoadSettings(settings);
    ctx->engine.SetParams(ctx->state->GetParamsSnapshot());

    if (ctx->state->IsDockVisible() && DockManager::Instance().IsReady()) {
        DockManager::Instance().OpenControlDock(ctx->state);
    }
}

static bool on_open_dock_clicked(obs_properties_t*, obs_property_t*, void* priv) {
    auto* ctx = static_cast<SurroundFilterContext*>(priv);
    if (ctx && ctx->state) {
        DockManager::Instance().OpenControlDock(ctx->state);
    }
    return false;
}

static void* filter_create(obs_data_t* settings, obs_source_t* context) {
    auto* ctx = new SurroundFilterContext();
    ctx->context = context;
    ctx->state = new FilterState(context);

    audio_t* audio = obs_get_audio();
    if (audio) {
        ctx->sample_rate = audio_output_get_sample_rate(audio);
    }
    ctx->engine.SetSampleRate(ctx->sample_rate);

    DockManager::Instance().RegisterFilterState(ctx->state);

    filter_update(ctx, settings);
    return ctx;
}

static struct obs_audio_data* filter_audio_process(void* data, struct obs_audio_data* audio) {
    auto* ctx = static_cast<SurroundFilterContext*>(data);
    if (!ctx || !ctx->state || !audio || audio->frames == 0) return audio;

    audio_t* master_audio = obs_get_audio();
    size_t obs_channels = master_audio ? audio_output_get_channels(master_audio) : 2;

    // Detect available input planes
    size_t input_planes = 0;
    for (size_t c = 0; c < 8; ++c) {
        if (c < MAX_AV_PLANES && audio->data[c]) {
            input_planes = c + 1;
        }
    }

    if (input_planes == 0) return audio;

    // Refresh engine sample rate if changed
    uint32_t current_sr = master_audio ? audio_output_get_sample_rate(master_audio) : 48000;
    if (current_sr != ctx->sample_rate) {
        ctx->sample_rate = current_sr;
        ctx->engine.SetSampleRate(current_sr);
    }

    // Process upmix
    UpmixParams p = ctx->state->GetParamsSnapshot();
    ctx->engine.SetParams(p);

    float* const* raw_planes = reinterpret_cast<float* const*>(audio->data);
    ctx->engine.Process(raw_planes, input_planes, obs_channels, audio->frames);

    // Feed level meter
    ctx->state->GetMeter().Process(raw_planes, obs_channels, audio->frames);

    return audio;
}

static void filter_defaults(obs_data_t* settings) {
    obs_data_set_default_int(settings, "mode", static_cast<long long>(UpmixMode::Auto));
    obs_data_set_default_double(settings, "gain_l", 0.0);
    obs_data_set_default_double(settings, "gain_r", 0.0);
    obs_data_set_default_double(settings, "gain_c", 0.0);
    obs_data_set_default_double(settings, "gain_lfe", 0.0);
    obs_data_set_default_double(settings, "gain_sl", 0.0);
    obs_data_set_default_double(settings, "gain_sr", 0.0);
    obs_data_set_default_double(settings, "gain_rl", 0.0);
    obs_data_set_default_double(settings, "gain_rr", 0.0);

    obs_data_set_default_double(settings, "pan_x", 0.0);
    obs_data_set_default_double(settings, "pan_y", 0.0);
    obs_data_set_default_double(settings, "center_spread", 0.5);
    obs_data_set_default_double(settings, "surround_spread", 0.7);
    obs_data_set_default_double(settings, "lfe_cutoff", 120.0);
}

static obs_properties_t* filter_properties(void* data) {
    auto* ctx = static_cast<SurroundFilterContext*>(data);
    obs_properties_t* props = obs_properties_create();

    // Button to open Sorround Control Dock
    obs_properties_add_button2(props, "open_dock", obs_module_text("Filter.OpenControlDock"), on_open_dock_clicked, ctx);

    // Mode
    obs_property_t* p_mode = obs_properties_add_list(
        props, "mode", obs_module_text("Filter.Mode"),
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(p_mode, obs_module_text("Filter.Mode.Auto"), static_cast<long long>(UpmixMode::Auto));
    obs_property_list_add_int(p_mode, obs_module_text("Filter.Mode.5Point1"), static_cast<long long>(UpmixMode::Force5Point1));
    obs_property_list_add_int(p_mode, obs_module_text("Filter.Mode.7Point1"), static_cast<long long>(UpmixMode::Force7Point1));
    obs_property_list_add_int(p_mode, obs_module_text("Filter.Mode.MonoSurround"), static_cast<long long>(UpmixMode::MonoSurround));
    obs_property_list_add_int(p_mode, obs_module_text("Filter.Mode.Direct"), static_cast<long long>(UpmixMode::Direct));

    // Panning
    obs_properties_add_float_slider(props, "pan_x", obs_module_text("Filter.PanX"), -1.0, 1.0, 0.05);
    obs_properties_add_float_slider(props, "pan_y", obs_module_text("Filter.PanY"), -1.0, 1.0, 0.05);

    // Spreads & LFE Cutoff
    obs_properties_add_float_slider(props, "center_spread", obs_module_text("Filter.CenterSpread"), 0.0, 1.0, 0.05);
    obs_properties_add_float_slider(props, "surround_spread", obs_module_text("Filter.SurroundSpread"), 0.0, 1.0, 0.05);

    obs_property_t* p_lfe = obs_properties_add_float_slider(props, "lfe_cutoff", obs_module_text("Filter.LfeCutoff"), 60.0, 250.0, 1.0);
    obs_property_float_set_suffix(p_lfe, " Hz");

    // Gains (-30 dB to +30 dB)
    auto add_gain_slider = [&](const char* name, const char* desc_key) {
        obs_property_t* p = obs_properties_add_float_slider(props, name, obs_module_text(desc_key), -30.0, 30.0, 0.1);
        obs_property_float_set_suffix(p, " dB");
    };

    add_gain_slider("gain_l",   "Filter.GainL");
    add_gain_slider("gain_r",   "Filter.GainR");
    add_gain_slider("gain_c",   "Filter.GainC");
    add_gain_slider("gain_lfe", "Filter.GainLFE");
    add_gain_slider("gain_sl",  "Filter.GainSL");
    add_gain_slider("gain_sr",  "Filter.GainSR");
    add_gain_slider("gain_rl",  "Filter.GainRL");
    add_gain_slider("gain_rr",  "Filter.GainRR");

    return props;
}

static void filter_save(void* data, obs_data_t* settings) {
    auto* ctx = static_cast<SurroundFilterContext*>(data);
    if (!ctx || !ctx->state) return;
    ctx->state->SaveSettings(settings);
}

struct obs_source_info surround_filter_info = {
    .id             = "aeria_surround_upmix_filter",
    .type           = OBS_SOURCE_TYPE_FILTER,
    .output_flags   = OBS_SOURCE_AUDIO,
    .get_name       = filter_get_name,
    .create         = filter_create,
    .destroy        = filter_destroy,
    .get_defaults   = filter_defaults,
    .get_properties = filter_properties,
    .update         = filter_update,
    .filter_audio   = filter_audio_process,
    .save           = filter_save,
};
