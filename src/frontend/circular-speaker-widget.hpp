#pragma once

#include <QWidget>
#include <QPointF>
#include <vector>
#include "../audio/channel-layout.hpp"
#include "../audio/level-meter.hpp"

namespace AeriaSurround {

class CircularSpeakerWidget : public QWidget {
    Q_OBJECT

public:
    explicit CircularSpeakerWidget(bool interactive = true, QWidget* parent = nullptr);

    void SetInteractive(bool interactive) { interactive_ = interactive; update(); }
    void SetPan(float x, float y);
    QPointF GetPan() const { return QPointF(pan_x_, pan_y_); }

    void SetChannelLevels(const ChannelMeterData levels[METER_MAX_CHANNELS], size_t active_channels);

signals:
    void PanChanged(float x, float y);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return QSize(280, 280); }
    QSize minimumSizeHint() const override { return QSize(180, 180); }

private:
    bool interactive_ = true;
    float pan_x_ = 0.0f; // [-1.0, 1.0]
    float pan_y_ = 0.0f; // [-1.0, 1.0]
    bool dragging_ = false;

    ChannelMeterData levels_[METER_MAX_CHANNELS];
    size_t active_channels_ = 8;

    void UpdatePanFromMouse(const QPointF& pos);
};

} // namespace AeriaSurround
