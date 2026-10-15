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

    void SetSoloState(int ch, bool latched = false);
    int GetSoloChannel() const { return solo_active_channel_; }
    bool IsSoloLatched() const { return solo_latched_; }

    void SetChannelLevels(const ChannelMeterData levels[METER_MAX_CHANNELS], size_t active_channels);

signals:
    void PanChanged(float x, float y);
    void SoloChannelPressed(ChannelIndex ch);
    void SoloChannelReleased();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    QSize sizeHint() const override { return QSize(180, 180); }
    QSize minimumSizeHint() const override { return QSize(100, 100); }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return w; }

private:
    bool interactive_ = true;
    float pan_x_ = 0.0f; // [-1.0, 1.0]
    float pan_y_ = 0.0f; // [-1.0, 1.0]
    bool dragging_ = false;
    int solo_active_channel_ = -1;
    bool solo_latched_ = false;
    // Incremented each time we want to cancel a pending deferred solo-release.
    // QTimer::singleShot lambda captures the value at scheduling time; if the
    // stored id no longer matches when the timer fires, the release is skipped.
    int solo_pending_release_id_ = 0;

    ChannelMeterData levels_[METER_MAX_CHANNELS];
    size_t active_channels_ = 8;

    void UpdatePanFromMouse(const QPointF& pos);
};

} // namespace AeriaSurround
