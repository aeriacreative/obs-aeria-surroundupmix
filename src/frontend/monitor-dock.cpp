#include "monitor-dock.hpp"
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QDesktopServices>
#include <QUrl>
#include <algorithm>

namespace AeriaSurround {

static const struct {
    ChannelIndex ch;
    const char* label;
    QColor color;
} MONITOR_CH_DEFS[8] = {
    { ChannelIndex::FrontLeft,  "L",   QColor(0, 210, 255) },
    { ChannelIndex::Center,     "C",   QColor(0, 255, 180) },
    { ChannelIndex::FrontRight, "R",   QColor(0, 210, 255) },
    { ChannelIndex::LFE,        "LFE", QColor(255, 140, 0) },
    { ChannelIndex::SideLeft,   "SL",  QColor(100, 180, 255) },
    { ChannelIndex::SideRight,  "SR",  QColor(100, 180, 255) },
    { ChannelIndex::RearLeft,   "RL",  QColor(140, 120, 255) },
    { ChannelIndex::RearRight,  "RR",  QColor(140, 120, 255) },
};

MonitorDockWidget::MonitorDockWidget(QWidget* parent)
    : QDockWidget(parent)
{
    setObjectName("obs_surround_monitor_dock");
    setWindowTitle("Sorround Monitor");

    BuildUI();
    UpdateLayoutInfo();
    StartMonitoring();

    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &MonitorDockWidget::OnTimer);
    timer_->start(33); // ~30 fps
}

MonitorDockWidget::~MonitorDockWidget() {
    StopMonitoring();
}

void MonitorDockWidget::StartMonitoring() {
    if (monitoring_connected_) return;

    audio_t* audio = obs_get_audio();
    if (audio) {
        monitoring_connected_ = audio_output_connect(audio, 0, nullptr, AudioCallback, this);
    }
}

void MonitorDockWidget::StopMonitoring() {
    if (!monitoring_connected_) return;

    audio_t* audio = obs_get_audio();
    if (audio) {
        audio_output_disconnect(audio, 0, AudioCallback, this);
    }
    monitoring_connected_ = false;
}

void MonitorDockWidget::AudioCallback(void* param, size_t, struct audio_data* data) {
    auto* self = static_cast<MonitorDockWidget*>(param);
    if (!self || !data) return;

    const float* planes[METER_MAX_CHANNELS] = {nullptr};
    size_t count = std::min(self->active_channels_, size_t(METER_MAX_CHANNELS));
    for (size_t i = 0; i < count; ++i) {
        planes[i] = reinterpret_cast<const float*>(data->data[i]);
    }

    self->meter_.Process(planes, count, data->frames);
}

void MonitorDockWidget::UpdateLayoutInfo() {
    audio_t* audio = obs_get_audio();
    if (audio) {
        const struct audio_output_info* info = audio_output_get_info(audio);
        if (info) {
            speaker_layout_ = info->speakers;
            active_channels_ = get_audio_channels(speaker_layout_);
        }
    }

    QString layout_str;
    switch (speaker_layout_) {
    case SPEAKERS_5POINT1:
        layout_str = "5.1 Surround (6 Channels Active)";
        break;
    case SPEAKERS_7POINT1:
        layout_str = "7.1 Surround (8 Channels Active)";
        break;
    case SPEAKERS_STEREO:
        layout_str = "Stereo (2 Channels Active — Surround channels inactive in OBS Settings)";
        break;
    case SPEAKERS_MONO:
        layout_str = "Mono (1 Channel Active)";
        break;
    default:
        layout_str = QString("Custom (%1 Channels)").arg(active_channels_);
        break;
    }

    if (header_layout_status_) {
        header_layout_status_->setText(QString("OBS Master Output: %1").arg(layout_str));
    }

    for (size_t i = 0; i < meter_cols_.size() && i < 8; ++i) {
        if (MONITOR_CH_DEFS[i].ch == ChannelIndex::SideLeft || MONITOR_CH_DEFS[i].ch == ChannelIndex::SideRight) {
            meter_cols_[i]->setVisible(active_channels_ >= 8);
        }
    }
}

void MonitorDockWidget::BuildUI() {
    auto* container = new QWidget(this);
    container->setStyleSheet(
        "QWidget {"
        "  background: #14161c;"
        "  color: #e0e4ed;"
        "  font-family: 'Segoe UI', 'SF Pro Text', sans-serif;"
        "}"
        "QPushButton {"
        "  background: #202534;"
        "  border: 1px solid #363e52;"
        "  border-radius: 3px;"
        "  padding: 4px 8px;"
        "  color: #c4cce0;"
        "}"
        "QPushButton:hover {"
        "  background: #293042;"
        "  border-color: #00d2ff;"
        "}"
        "QPushButton:checked {"
        "  background: #0088aa;"
        "  border-color: #00d2ff;"
        "  color: #ffffff;"
        "}"
    );

    auto* root_layout = new QVBoxLayout(container);
    root_layout->setContentsMargins(8, 8, 8, 8);
    root_layout->setSpacing(6);

    // Header
    auto* header_layout = new QHBoxLayout();
    header_layout->setSpacing(6);

    auto* title_box = new QVBoxLayout();
    title_box->setSpacing(1);
    header_title_ = new QLabel("Sorround Monitor", container);
    header_title_->setStyleSheet("font-weight: bold; font-size: 13px; color: #ffffff;");
    header_title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    header_title_->setMinimumWidth(0);

    header_source_info_ = new QLabel("Point: OBS Master Audio Output (Track 1 Mix)", container);
    header_source_info_->setStyleSheet("color: #adb5c4; font-size: 11px;");
    header_source_info_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    header_source_info_->setMinimumWidth(0);

    header_layout_status_ = new QLabel("OBS Master Output: Detecting...", container);
    header_layout_status_->setStyleSheet("color: #00d2ff; font-size: 11px;");
    header_layout_status_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    header_layout_status_->setMinimumWidth(0);

    title_box->addWidget(header_title_);
    title_box->addWidget(header_source_info_);
    title_box->addWidget(header_layout_status_);
    header_layout->addLayout(title_box, 1);

    meter_btn_ = new QPushButton("Preview Meter", container);
    meter_btn_->setCheckable(true);
    meter_btn_->setChecked(true);
    connect(meter_btn_, &QPushButton::toggled, this, &MonitorDockWidget::OnToggleMeter);
    header_layout->addWidget(meter_btn_);

    root_layout->addLayout(header_layout);

    // Separator line
    auto* sep1 = new QFrame(container);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet("color: #242833;");
    root_layout->addWidget(sep1);

    // Circular Speaker Monitor View (non-interactive)
    panner_ = new CircularSpeakerWidget(false, container);
    root_layout->addWidget(panner_, 1, Qt::AlignCenter);

    // Separator line
    auto* sep2 = new QFrame(container);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet("color: #242833;");
    root_layout->addWidget(sep2);

    // Channel Meters Strip Row
    auto* meters_container = new QWidget(container);
    auto* meters_layout = new QHBoxLayout(meters_container);
    meters_layout->setContentsMargins(0, 0, 0, 0);
    meters_layout->setSpacing(4);

    meter_cols_.clear();
    meter_bars_.clear();
    meter_labels_.clear();

    for (size_t i = 0; i < 8; ++i) {
        auto* col_widget = new QWidget(meters_container);
        auto* col = new QVBoxLayout(col_widget);
        col->setContentsMargins(0, 0, 0, 0);
        col->setSpacing(2);

        auto* label = new QLabel(MONITOR_CH_DEFS[i].label, col_widget);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet(QString(
            "QLabel {"
            "  font-weight: bold;"
            "  font-size: 11px;"
            "  color: %1;"
            "  background: #181b22;"
            "  border: 1px solid #282d38;"
            "  border-radius: 3px;"
            "  padding: 2px;"
            "}").arg(MONITOR_CH_DEFS[i].color.name()));
        col->addWidget(label);

        auto* bar = new MeterBarWidget(col_widget);
        col->addWidget(bar, 1);

        auto* db_text = new QLabel("-inf", col_widget);
        db_text->setAlignment(Qt::AlignCenter);
        db_text->setStyleSheet("font-size: 9px; color: #8892a4;");
        col->addWidget(db_text);

        meters_layout->addWidget(col_widget);
        meter_cols_.push_back(col_widget);
        meter_bars_.push_back(bar);
        meter_labels_.push_back(db_text);
    }
    root_layout->addWidget(meters_container);

    // Separator line
    auto* sep3 = new QFrame(container);
    sep3->setFrameShape(QFrame::HLine);
    sep3->setStyleSheet("color: #242833;");
    root_layout->addWidget(sep3);

    // Footer with Clickable Credit
    credit_label_ = new QLabel(
        "<a href=\"https://linktr.ee/aeriacreative\" style=\"color: #00d2ff; text-decoration: none;\">"
        "Aeria Sorround Upmix · linktr.ee/aeriacreative</a>",
        container);
    credit_label_->setAlignment(Qt::AlignCenter);
    credit_label_->setTextFormat(Qt::RichText);
    credit_label_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    credit_label_->setOpenExternalLinks(true);
    credit_label_->setStyleSheet("font-size: 11px; padding: 2px;");
    credit_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    credit_label_->setMinimumWidth(0);
    root_layout->addWidget(credit_label_);

    auto* scroll_area = new QScrollArea(this);
    scroll_area->setWidgetResizable(true);
    scroll_area->setFrameShape(QFrame::NoFrame);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_area->setWidget(container);

    setWidget(scroll_area);
    setMinimumWidth(160);
    setMinimumHeight(200);
}

void MonitorDockWidget::OnTimer() {
    if (!meter_enabled_) return;

    UpdateLayoutInfo();

    ChannelMeterData levels[METER_MAX_CHANNELS];
    meter_.GetSnapshot(levels, METER_MAX_CHANNELS);

    // Update circular view
    panner_->SetChannelLevels(levels, active_channels_);

    // Update channel meters
    for (size_t i = 0; i < 8; ++i) {
        size_t ch_idx = static_cast<size_t>(MONITOR_CH_DEFS[i].ch);
        if (ch_idx < METER_MAX_CHANNELS && i < meter_bars_.size()) {
            bool active = (ch_idx < active_channels_);
            meter_bars_[i]->setEnabled(active);
            meter_bars_[i]->SetLevel(levels[ch_idx].peak_db, levels[ch_idx].rms_db, levels[ch_idx].clip);

            float peak_db = levels[ch_idx].peak_db;
            if (active && peak_db > -59.0f) {
                meter_labels_[i]->setText(QString("%1").arg(peak_db, 0, 'f', 1));
            } else {
                meter_labels_[i]->setText(active ? "-inf" : "off");
            }
        }
    }
}

void MonitorDockWidget::OnToggleMeter(bool checked) {
    meter_enabled_ = checked;
    meter_btn_->setText(checked ? "Preview Meter" : "Meter Paused");
    if (!checked) {
        ChannelMeterData zero_levels[METER_MAX_CHANNELS];
        panner_->SetChannelLevels(zero_levels, active_channels_);
        for (auto* bar : meter_bars_) {
            bar->SetLevel(-60.0f, -60.0f, false);
        }
    }
}

} // namespace AeriaSurround
