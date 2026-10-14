#pragma once

#include <QDockWidget>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
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

private:
    LevelMeter meter_;
    bool monitoring_connected_ = false;
    bool meter_enabled_ = true;
    QTimer* timer_ = nullptr;

    size_t active_channels_ = 2;
    speaker_layout speaker_layout_ = SPEAKERS_UNKNOWN;

    // UI elements
    QLabel* header_title_ = nullptr;
    QLabel* header_source_info_ = nullptr;
    QLabel* header_layout_status_ = nullptr;
    QPushButton* meter_btn_ = nullptr;

    CircularSpeakerWidget* panner_ = nullptr;
    std::vector<MeterBarWidget*> meter_bars_;
    std::vector<QLabel*> meter_labels_;

    QLabel* credit_label_ = nullptr;

    void BuildUI();
    void UpdateLayoutInfo();

    static void AudioCallback(void* param, size_t mix_idx, struct audio_data* data);
};

} // namespace AeriaSurround
