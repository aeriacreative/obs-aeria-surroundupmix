#pragma once

#include <QDockWidget>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <vector>
#include "circular-speaker-widget.hpp"
#include "channel-strip-widget.hpp"
#include "../audio/level-meter.hpp"
#include <obs.h>
#include <media-io/audio-io.h>

namespace AeriaSurround {

class MonitorDockWidget : public QDockWidget {
    Q_OBJECT

public:
    explicit MonitorDockWidget(QWidget* parent = nullptr);
    ~MonitorDockWidget() override;

    void StartMonitoring();
    void StopMonitoring();

private slots:
    void OnTimer();
    void OnToggleMeter(bool checked);
    void OnToggleGlobalSurround(bool checked);
    void OnModeChanged(int index);
    void OnCenterSpreadChanged(int val);
    void OnSurroundSpreadChanged(int val);
    void OnLfeCutoffChanged(int val);
    void OnPanChanged(float x, float y);
    void OnResetClicked();

private:
    LevelMeter meter_;
    bool monitoring_connected_ = false;
    bool meter_enabled_ = true;
    QTimer* timer_ = nullptr;

    size_t active_channels_ = 2;
    speaker_layout speaker_layout_ = SPEAKERS_UNKNOWN;

    // Header elements
    QLabel* header_title_ = nullptr;
    QLabel* header_source_info_ = nullptr;
    QLabel* header_layout_status_ = nullptr;
    QPushButton* global_surround_btn_ = nullptr;
    QPushButton* meter_btn_ = nullptr;

    // Circular Speaker Panner / Monitor
    CircularSpeakerWidget* panner_ = nullptr;

    // Passive Meter Section (Global Surround OFF)
    QWidget* passive_meters_container_ = nullptr;
    std::vector<QWidget*> meter_cols_;
    std::vector<MeterBarWidget*> meter_bars_;
    std::vector<QLabel*> meter_labels_;

    // Active Global Controls Section (Global Surround ON)
    QWidget* global_controls_container_ = nullptr;
    QComboBox* mode_combo_ = nullptr;
    QSlider* center_spread_slider_ = nullptr;
    QSlider* surround_spread_slider_ = nullptr;
    QSlider* lfe_cutoff_slider_ = nullptr;
    QLabel* center_spread_val_ = nullptr;
    QLabel* surround_spread_val_ = nullptr;
    QLabel* lfe_cutoff_val_ = nullptr;

    ChannelStripWidget* strips_[8] = {nullptr};
    QPushButton* reset_btn_ = nullptr;

    QLabel* credit_label_ = nullptr;

    void BuildUI();
    void UpdateLayoutInfo();
    void LoadFromGlobalState();
    void SetSoloChannel(int ch_idx, bool solo, bool latched = false);

    static void AudioCallback(void* param, size_t mix_idx, struct audio_data* data);
};

} // namespace AeriaSurround
