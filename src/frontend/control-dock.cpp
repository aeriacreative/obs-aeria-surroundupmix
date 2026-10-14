#include "control-dock.hpp"
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QFrame>
#include <QApplication>
#include <obs-module.h>

namespace AeriaSurround {

static const struct {
    ChannelIndex ch;
    const char* label;
    QColor color;
} STRIP_DEFS[8] = {
    { ChannelIndex::FrontLeft,  "L",   QColor(0, 210, 255) },
    { ChannelIndex::Center,     "C",   QColor(0, 255, 180) },
    { ChannelIndex::FrontRight, "R",   QColor(0, 210, 255) },
    { ChannelIndex::LFE,        "LFE", QColor(255, 140, 0) },
    { ChannelIndex::SideLeft,   "SL",  QColor(100, 180, 255) },
    { ChannelIndex::SideRight,  "SR",  QColor(100, 180, 255) },
    { ChannelIndex::RearLeft,   "RL",  QColor(140, 120, 255) },
    { ChannelIndex::RearRight,  "RR",  QColor(140, 120, 255) },
};

ControlDockWidget::ControlDockWidget(FilterState* filter_state, QWidget* parent)
    : QDockWidget(parent), filter_state_(filter_state)
{
    if (filter_state_) {
        instance_id_ = filter_state_->GetInstanceId();
    }

    setObjectName(QString::fromStdString(instance_id_));
    RefreshTitle();

    BuildUI();
    LoadFromState();

    if (filter_state_) {
        filter_state_->SetChangeCallback([this](const UpmixParams&) {
            QMetaObject::invokeMethod(this, [this]() {
                LoadFromState();
            }, Qt::QueuedConnection);
        });
    }

    meter_timer_ = new QTimer(this);
    connect(meter_timer_, &QTimer::timeout, this, &ControlDockWidget::OnMeterTimer);
    meter_timer_->start(33); // ~30 fps
}

ControlDockWidget::~ControlDockWidget() {
    DetachFilter();
}

void ControlDockWidget::DetachFilter() {
    if (filter_state_) {
        filter_state_->SetChangeCallback(nullptr);
        filter_state_ = nullptr;
    }
    if (header_status_) {
        header_status_->setText("(Filter Disconnected)");
        header_status_->setStyleSheet("color: #ff5555; font-size: 11px;");
    }
}

void ControlDockWidget::RefreshTitle() {
    if (filter_state_) {
        std::string disp = filter_state_->GetDisplayName() + " — Sorround Control";
        setWindowTitle(QString::fromUtf8(disp.c_str()));
        if (header_title_) {
            header_title_->setText(QString::fromUtf8(disp.c_str()));
        }
    } else {
        setWindowTitle("Sorround Control");
    }
}

void ControlDockWidget::BuildUI() {
    auto* container = new QWidget(this);
    container->setStyleSheet(
        "QWidget {"
        "  background: #14161c;"
        "  color: #e0e4ed;"
        "  font-family: 'Segoe UI', 'SF Pro Text', sans-serif;"
        "}"
        "QComboBox {"
        "  background: #1c202a;"
        "  border: 1px solid #2e3544;"
        "  border-radius: 3px;"
        "  padding: 3px 6px;"
        "  color: #c4cce0;"
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
    header_title_ = new QLabel(windowTitle(), container);
    header_title_->setStyleSheet("font-weight: bold; font-size: 13px; color: #ffffff;");
    header_status_ = new QLabel("Aeria Sorround Upmix (5.1 / 7.1 DSP)", container);
    header_status_->setStyleSheet("color: #00d2ff; font-size: 11px;");
    title_box->addWidget(header_title_);
    title_box->addWidget(header_status_);
    header_layout->addLayout(title_box, 1);

    meter_btn_ = new QPushButton("Preview Meter", container);
    meter_btn_->setCheckable(true);
    meter_btn_->setChecked(true);
    connect(meter_btn_, &QPushButton::toggled, this, &ControlDockWidget::OnToggleMeter);
    header_layout->addWidget(meter_btn_);

    root_layout->addLayout(header_layout);

    // Separator line
    auto* sep1 = new QFrame(container);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet("color: #242833;");
    root_layout->addWidget(sep1);

    // Mode Selector row
    auto* mode_row = new QHBoxLayout();
    mode_row->addWidget(new QLabel("Mode:", container));
    mode_combo_ = new QComboBox(container);
    mode_combo_->addItem("Auto (Detect OBS Output)");
    mode_combo_->addItem("Force 5.1 Surround");
    mode_combo_->addItem("Force 7.1 Surround");
    mode_combo_->addItem("Direct Pass-Through");
    connect(mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ControlDockWidget::OnModeChanged);
    mode_row->addWidget(mode_combo_, 1);
    root_layout->addLayout(mode_row);

    // Panner Section
    panner_ = new CircularSpeakerWidget(true, container);
    connect(panner_, &CircularSpeakerWidget::PanChanged, this, &ControlDockWidget::OnPanChanged);
    root_layout->addWidget(panner_, 1, Qt::AlignCenter);

    pan_label_ = new QLabel("Pan: Center (0.00, 0.00) — Drag handle or double-click to center", container);
    pan_label_->setAlignment(Qt::AlignCenter);
    pan_label_->setStyleSheet("color: #8c96ab; font-size: 11px;");
    root_layout->addWidget(pan_label_);

    // Separator line
    auto* sep2 = new QFrame(container);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet("color: #242833;");
    root_layout->addWidget(sep2);

    // Channel Fader & Meter Strips
    auto* strips_container = new QWidget(container);
    auto* strips_layout = new QHBoxLayout(strips_container);
    strips_layout->setContentsMargins(0, 0, 0, 0);
    strips_layout->setSpacing(4);

    strips_.clear();
    for (size_t i = 0; i < 8; ++i) {
        auto* strip = new ChannelStripWidget(
            STRIP_DEFS[i].ch,
            STRIP_DEFS[i].label,
            STRIP_DEFS[i].color,
            strips_container
        );
        connect(strip, &ChannelStripWidget::GainChanged, this, &ControlDockWidget::OnGainChanged);
        strips_layout->addWidget(strip);
        strips_.push_back(strip);
    }
    root_layout->addWidget(strips_container);

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
    root_layout->addWidget(credit_label_);

    setWidget(container);
    setMinimumWidth(320);
    setMinimumHeight(480);
}

void ControlDockWidget::LoadFromState() {
    if (!filter_state_) return;
    updating_from_state_ = true;

    UpmixParams p = filter_state_->GetParams();
    mode_combo_->setCurrentIndex(static_cast<int>(p.mode));

    panner_->SetPan(p.pan_x, p.pan_y);
    pan_label_->setText(QString("Pan: X = %1, Y = %2 (Double-click to center)")
        .arg(p.pan_x, 5, 'f', 2)
        .arg(p.pan_y, 5, 'f', 2));

    for (auto* strip : strips_) {
        size_t idx = static_cast<size_t>(strip->GetChannel());
        if (idx < 8) {
            strip->SetGainDb(p.channel_gain_db[idx]);
        }
    }

    RefreshTitle();
    updating_from_state_ = false;
}

void ControlDockWidget::OnMeterTimer() {
    if (!filter_state_ || !meter_enabled_) return;

    ChannelMeterData levels[METER_MAX_CHANNELS];
    filter_state_->GetMeter().GetSnapshot(levels, METER_MAX_CHANNELS);

    // Update panner visualizer
    panner_->SetChannelLevels(levels, 8);

    // Update each channel strip meter
    for (auto* strip : strips_) {
        size_t idx = static_cast<size_t>(strip->GetChannel());
        if (idx < METER_MAX_CHANNELS) {
            strip->SetMeterData(levels[idx]);
        }
    }
}

void ControlDockWidget::OnPanChanged(float x, float y) {
    if (updating_from_state_ || !filter_state_) return;
    pan_label_->setText(QString("Pan: X = %1, Y = %2 (Double-click to center)")
        .arg(x, 5, 'f', 2)
        .arg(y, 5, 'f', 2));
    filter_state_->SetPanning(x, y);
}

void ControlDockWidget::OnGainChanged(ChannelIndex ch, float db) {
    if (updating_from_state_ || !filter_state_) return;
    filter_state_->SetChannelGain(ch, db);
}

void ControlDockWidget::OnToggleMeter(bool checked) {
    meter_enabled_ = checked;
    meter_btn_->setText(checked ? "Preview Meter" : "Meter Paused");
    if (!checked) {
        // Clear meter displays
        ChannelMeterData zero_levels[METER_MAX_CHANNELS];
        panner_->SetChannelLevels(zero_levels, 8);
        for (auto* strip : strips_) {
            strip->SetMeterData(ChannelMeterData());
        }
    }
}

void ControlDockWidget::OnModeChanged(int index) {
    if (updating_from_state_ || !filter_state_) return;
    filter_state_->SetMode(static_cast<UpmixMode>(index));
}

void ControlDockWidget::OnCreditClicked() {
    QDesktopServices::openUrl(QUrl("https://linktr.ee/aeriacreative"));
}

} // namespace AeriaSurround
