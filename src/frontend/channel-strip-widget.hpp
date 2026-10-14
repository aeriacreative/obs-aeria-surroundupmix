#pragma once

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include "../audio/channel-layout.hpp"
#include "../audio/level-meter.hpp"

namespace AeriaSurround {

class MeterBarWidget : public QWidget {
    Q_OBJECT

public:
    explicit MeterBarWidget(QWidget* parent = nullptr);
    void SetLevel(float peak_db, float rms_db, bool clip);

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override { return QSize(14, 120); }
    QSize minimumSizeHint() const override { return QSize(10, 80); }

private:
    float peak_db_ = -60.0f;
    float rms_db_  = -60.0f;
    bool  clip_    = false;
};

class ChannelStripWidget : public QWidget {
    Q_OBJECT

public:
    explicit ChannelStripWidget(ChannelIndex ch, const QString& label, const QColor& color, QWidget* parent = nullptr);

    ChannelIndex GetChannel() const { return ch_; }
    void SetGainDb(float db);
    float GetGainDb() const;

    void SetMeterData(const ChannelMeterData& data);
    void SetChannelActive(bool active);

signals:
    void GainChanged(ChannelIndex ch, float db);

private:
    ChannelIndex ch_;
    QColor color_;

    QLabel* title_label_ = nullptr;
    MeterBarWidget* meter_ = nullptr;
    QSlider* fader_ = nullptr;
    QLabel* db_label_ = nullptr;
    bool updating_ui_ = false;

    void OnSliderValueChanged(int value);
};

} // namespace AeriaSurround
