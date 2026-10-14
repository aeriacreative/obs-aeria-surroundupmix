#include "channel-strip-widget.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <cmath>
#include <algorithm>

namespace AeriaSurround {

// Custom slider that resets to 0 on double click
class ResetSlider : public QSlider {
public:
    using QSlider::QSlider;
protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            setValue(0);
            event->accept();
            return;
        }
        QSlider::mouseDoubleClickEvent(event);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// MeterBarWidget
// ─────────────────────────────────────────────────────────────────────────────
MeterBarWidget::MeterBarWidget(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

void MeterBarWidget::SetLevel(float peak_db, float rms_db, bool clip) {
    peak_db_ = peak_db;
    rms_db_  = rms_db;
    clip_    = clip;
    update();
}

void MeterBarWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    int w = width();
    int h = height();

    // Background
    p.fillRect(0, 0, w, h, QColor(14, 16, 20));
    p.setPen(QColor(32, 36, 44));
    p.drawRect(0, 0, w - 1, h - 1);

    int clip_h = 4;
    int meter_top = clip_h + 2;
    int meter_h = h - meter_top - 2;
    if (meter_h < 4) return;

    // Draw Clip LED at top
    p.fillRect(1, 1, w - 2, clip_h, clip_ ? QColor(255, 30, 30) : QColor(40, 15, 15));

    // Convert dBFS [-60.0 to 0.0] to normalized height [0.0 to 1.0]
    float norm_peak = std::clamp((peak_db_ + 60.0f) / 60.0f, 0.0f, 1.0f);
    float norm_rms  = std::clamp((rms_db_  + 60.0f) / 60.0f, 0.0f, 1.0f);

    int rms_fill_h  = static_cast<int>(norm_rms * meter_h);
    int peak_line_y = meter_top + meter_h - static_cast<int>(norm_peak * meter_h);

    // Meter gradient fill (RMS)
    if (rms_fill_h > 0) {
        int fill_y = meter_top + meter_h - rms_fill_h;
        QLinearGradient grad(0, meter_top + meter_h, 0, meter_top);
        grad.setColorAt(0.0f, QColor(0, 220, 100));   // -60 dB (green)
        grad.setColorAt(0.70f, QColor(160, 220, 0));  // -18 dB (lime)
        grad.setColorAt(0.88f, QColor(255, 180, 0));  // -6 dB (amber)
        grad.setColorAt(1.0f, QColor(255, 50, 50));   // 0 dB (red)

        p.fillRect(2, fill_y, w - 4, rms_fill_h, grad);
    }

    // Peak needle line
    if (norm_peak > 0.02f) {
        p.setPen(QColor(255, 255, 255, 230));
        p.drawLine(1, peak_line_y, w - 2, peak_line_y);
    }
}

// Clickable label for solo preview
class SoloLabel : public QLabel {
public:
    std::function<void()> on_pressed;
    std::function<void()> on_released;
    using QLabel::QLabel;
protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            if (on_pressed) on_pressed();
            event->accept();
            return;
        }
        QLabel::mousePressEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            if (on_released) on_released();
            event->accept();
            return;
        }
        QLabel::mouseReleaseEvent(event);
    }
    void leaveEvent(QEvent* event) override {
        QLabel::leaveEvent(event);
        if (on_released) on_released();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// ChannelStripWidget
// ─────────────────────────────────────────────────────────────────────────────
ChannelStripWidget::ChannelStripWidget(ChannelIndex ch, const QString& label, const QColor& color, QWidget* parent)
    : QWidget(parent), ch_(ch), color_(color)
{
    setObjectName("ChannelStrip");
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // Channel label (interactive solo preview)
    auto* solo_lbl = new SoloLabel(label, this);
    solo_lbl->setAlignment(Qt::AlignCenter);
    solo_lbl->setCursor(Qt::PointingHandCursor);
    solo_lbl->setToolTip("Click & hold to solo preview channel");
    solo_lbl->setStyleSheet(QString(
        "QLabel {"
        "  font-weight: bold;"
        "  font-size: 11px;"
        "  color: %1;"
        "  background: #181b22;"
        "  border: 1px solid #282d38;"
        "  border-radius: 3px;"
        "  padding: 2px;"
        "}").arg(color.name()));
    solo_lbl->on_pressed = [this]() {
        SetSoloHighlighted(true);
        emit SoloPressed(ch_);
    };
    solo_lbl->on_released = [this]() {
        SetSoloHighlighted(false);
        emit SoloReleased();
    };
    title_label_ = solo_lbl;
    layout->addWidget(title_label_);

    // Meter and Fader side by side
    auto* controls_row = new QHBoxLayout();
    controls_row->setContentsMargins(0, 0, 0, 0);
    controls_row->setSpacing(3);

    meter_ = new MeterBarWidget(this);
    controls_row->addWidget(meter_);

    // Fader: -30.0 dB to +30.0 dB -> -300 to +300
    fader_ = new ResetSlider(Qt::Vertical, this);
    fader_->setRange(-300, 300);
    fader_->setValue(0);
    fader_->setTickInterval(60); // tick every 6 dB
    fader_->setTickPosition(QSlider::TicksRight);
    fader_->setToolTip("Double-click to reset to 0 dB");
    fader_->setStyleSheet(QString(
        "QSlider::groove:vertical {"
        "  background: #15181e;"
        "  width: 6px;"
        "  border-radius: 3px;"
        "}"
        "QSlider::sub-page:vertical {"
        "  background: #20242e;"
        "  border-radius: 3px;"
        "}"
        "QSlider::add-page:vertical {"
        "  background: %1;"
        "  border-radius: 3px;"
        "}"
        "QSlider::handle:vertical {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #dce2ee, stop:1 #707888);"
        "  border: 1px solid #101216;"
        "  height: 12px;"
        "  margin: 0 -4px;"
        "  border-radius: 2px;"
        "}").arg(color.name()));
    controls_row->addWidget(fader_);

    layout->addLayout(controls_row, 1);

    // Numeric dB readout
    db_label_ = new QLabel("0.0 dB", this);
    db_label_->setAlignment(Qt::AlignCenter);
    db_label_->setStyleSheet(
        "QLabel {"
        "  font-size: 10px;"
        "  color: #adb5c4;"
        "  background: #121418;"
        "  border-radius: 2px;"
        "  padding: 1px;"
        "}");
    layout->addWidget(db_label_);

    connect(fader_, &QSlider::valueChanged, this, &ChannelStripWidget::OnSliderValueChanged);
}

void ChannelStripWidget::SetGainDb(float db) {
    updating_ui_ = true;
    int val = static_cast<int>(std::round(db * 10.0f));
    val = std::clamp(val, -300, 300);
    fader_->setValue(val);
    db_label_->setText(QString("%1%2 dB").arg(db > 0.001f ? "+" : "").arg(db, 0, 'f', 1));
    updating_ui_ = false;
}

float ChannelStripWidget::GetGainDb() const {
    return fader_->value() / 10.0f;
}

void ChannelStripWidget::SetMeterData(const ChannelMeterData& data) {
    meter_->SetLevel(data.peak_db, data.rms_db, data.clip);
}

void ChannelStripWidget::SetChannelActive(bool active) {
    setEnabled(active);
    title_label_->setStyleSheet(QString(
        "QLabel {"
        "  font-weight: bold;"
        "  font-size: 11px;"
        "  color: %1;"
        "  background: #181b22;"
        "  border: 1px solid #282d38;"
        "  border-radius: 3px;"
        "  padding: 2px;"
        "}").arg(active ? color_.name() : "#505664"));
}

void ChannelStripWidget::SetSoloHighlighted(bool soloed) {
    if (soloed) {
        title_label_->setStyleSheet(QString(
            "QLabel {"
            "  font-weight: bold;"
            "  font-size: 11px;"
            "  color: #101216;"
            "  background: %1;"
            "  border: 1px solid #ffffff;"
            "  border-radius: 3px;"
            "  padding: 2px;"
            "}").arg(color_.name()));
    } else {
        SetChannelActive(isEnabled());
    }
}

void ChannelStripWidget::OnSliderValueChanged(int value) {
    float db = value / 10.0f;
    db_label_->setText(QString("%1%2 dB").arg(db > 0.001f ? "+" : "").arg(db, 0, 'f', 1));
    if (!updating_ui_) {
        emit GainChanged(ch_, db);
    }
}

} // namespace AeriaSurround
