#include "circular-speaker-widget.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <cmath>
#include <algorithm>

namespace AeriaSurround {

static constexpr float PI = 3.14159265358979323846f;

struct SpeakerPos {
    ChannelIndex ch;
    const char* label;
    float nx; // normalized x [-1, 1]
    float ny; // normalized y [-1, 1] (+y is front/top)
    QColor base_color;
};

static const SpeakerPos SPEAKER_LAYOUT[8] = {
    { ChannelIndex::FrontLeft,  "L",   -0.50f,  0.866f, QColor(0, 210, 255) },  // ~ -30 deg
    { ChannelIndex::FrontRight, "R",    0.50f,  0.866f, QColor(0, 210, 255) },  // ~ +30 deg
    { ChannelIndex::Center,     "C",    0.00f,  0.95f,  QColor(0, 255, 180) },  // 0 deg (front center)
    { ChannelIndex::LFE,        "LFE",  0.00f,  0.42f,  QColor(255, 140, 0) },  // Subwoofer
    { ChannelIndex::RearLeft,   "RL",  -0.50f, -0.866f, QColor(140, 120, 255) },// ~ -150 deg
    { ChannelIndex::RearRight,  "RR",   0.50f, -0.866f, QColor(140, 120, 255) },// ~ +150 deg
    { ChannelIndex::SideLeft,   "SL",  -0.92f,  0.00f,  QColor(100, 180, 255) },// ~ -90 deg
    { ChannelIndex::SideRight,  "SR",   0.92f,  0.00f,  QColor(100, 180, 255) },// ~ +90 deg
};

CircularSpeakerWidget::CircularSpeakerWidget(bool interactive, QWidget* parent)
    : QWidget(parent), interactive_(interactive)
{
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    for (size_t i = 0; i < METER_MAX_CHANNELS; ++i) {
        levels_[i].peak_linear = 0.0f;
        levels_[i].clip = false;
    }
}

void CircularSpeakerWidget::SetPan(float x, float y) {
    pan_x_ = std::clamp(x, -1.0f, 1.0f);
    pan_y_ = std::clamp(y, -1.0f, 1.0f);
    update();
}

void CircularSpeakerWidget::SetSoloState(int ch, bool latched) {
    solo_active_channel_ = ch;
    solo_latched_ = latched;
    update();
}

void CircularSpeakerWidget::SetChannelLevels(const ChannelMeterData levels[METER_MAX_CHANNELS], size_t active_channels) {
    active_channels_ = active_channels;
    for (size_t i = 0; i < METER_MAX_CHANNELS; ++i) {
        levels_[i] = levels[i];
    }
    update();
}

void CircularSpeakerWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    float center_x = w * 0.5f;
    float center_y = h * 0.5f;
    float radius = std::min(w, h) * 0.44f;

    // Background circle (Dark VST metallic plate)
    QRadialGradient bg_grad(center_x, center_y, radius);
    bg_grad.setColorAt(0.0f, QColor(24, 27, 34));
    bg_grad.setColorAt(0.85f, QColor(16, 18, 22));
    bg_grad.setColorAt(1.0f, QColor(10, 11, 14));

    p.setBrush(bg_grad);
    p.setPen(QPen(QColor(42, 47, 58), 2.0));
    p.drawEllipse(QPointF(center_x, center_y), radius, radius);

    // Concentric grid rings
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(36, 40, 50), 1.0, Qt::DashLine));
    p.drawEllipse(QPointF(center_x, center_y), radius * 0.33f, radius * 0.33f);
    p.drawEllipse(QPointF(center_x, center_y), radius * 0.66f, radius * 0.66f);

    // Crosshairs
    p.setPen(QPen(QColor(34, 38, 48), 1.0));
    p.drawLine(QPointF(center_x - radius, center_y), QPointF(center_x + radius, center_y));
    p.drawLine(QPointF(center_x, center_y - radius), QPointF(center_x, center_y + radius));

    // Center origin dot
    p.setBrush(QColor(55, 62, 75));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(center_x, center_y), 3.0, 3.0);

    // Draw Speaker Nodes
    float node_r = std::max(11.0f, radius * 0.095f);
    QFont font = p.font();
    font.setPixelSize(static_cast<int>(node_r * 0.95f));
    font.setBold(true);
    p.setFont(font);

    for (size_t i = 0; i < 8; ++i) {
        const auto& sp = SPEAKER_LAYOUT[i];
        size_t ch_idx = static_cast<size_t>(sp.ch);

        // Hide SL and SR if active_channels_ <= 6 (5.1 surround)
        if (active_channels_ <= 6 && (sp.ch == ChannelIndex::SideLeft || sp.ch == ChannelIndex::SideRight)) {
            continue;
        }

        // Determine if this channel is active in OBS
        bool is_active = (ch_idx < active_channels_);
        bool is_soloed = (solo_active_channel_ == static_cast<int>(sp.ch));
        float sp_x = center_x + sp.nx * (radius * 0.82f);
        float sp_y = center_y - sp.ny * (radius * 0.82f);

        float level = (ch_idx < METER_MAX_CHANNELS) ? levels_[ch_idx].peak_linear : 0.0f;
        bool clip   = (ch_idx < METER_MAX_CHANNELS) && levels_[ch_idx].clip;

        // Draw audio energy glow halo — brighter and more responsive
        if (is_active && (level > 0.005f || is_soloed)) {
            float glow_radius = node_r + (is_soloed ? node_r * 2.2f
                                                     : std::min(level * 55.0f, node_r * 3.0f));
            QRadialGradient glow(sp_x, sp_y, glow_radius);
            QColor glow_col = is_soloed ? QColor(0, 255, 255)
                                        : (clip ? QColor(255, 40, 40) : sp.base_color);
            int alpha = is_soloed ? 255
                                  : static_cast<int>(std::min(255.0f, level * 400.0f + 40.0f));
            glow_col.setAlpha(alpha);
            glow.setColorAt(0.0f, glow_col);
            glow_col.setAlpha(0);
            glow.setColorAt(1.0f, glow_col);
            p.setBrush(glow);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(sp_x, sp_y), glow_radius, glow_radius);
        }

        // Speaker node circle
        QColor node_bg = is_soloed ? QColor(0, 120, 180) : (is_active ? QColor(28, 32, 42) : QColor(20, 22, 26));
        QColor border_col = is_soloed ? QColor(255, 255, 255) : (is_active ? (clip ? QColor(255, 60, 60) : sp.base_color) : QColor(50, 54, 64));
        if (is_active && level > 0.1f && !is_soloed) {
            border_col = border_col.lighter(130);
        }

        p.setBrush(node_bg);
        p.setPen(QPen(border_col, (is_active || is_soloed) ? 2.5 : 1.0));
        p.drawEllipse(QPointF(sp_x, sp_y), node_r, node_r);

        // Latched solo dashed ring indicator
        if (is_soloed && solo_latched_) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(0, 255, 255, 240), 1.5, Qt::DashLine));
            p.drawEllipse(QPointF(sp_x, sp_y), node_r * 1.38f, node_r * 1.38f);
        }

        // Text label
        p.setPen(is_soloed ? QColor(255, 255, 255) : (is_active ? (clip ? QColor(255, 100, 100) : sp.base_color) : QColor(70, 75, 88)));
        QRectF text_rect(sp_x - node_r, sp_y - node_r, node_r * 2.0f, node_r * 2.0f);
        p.drawText(text_rect, Qt::AlignCenter, sp.label);
    }

    // Draw Panning Puck / Handle
    if (interactive_) {
        float puck_x = center_x + pan_x_ * (radius * 0.78f);
        float puck_y = center_y - pan_y_ * (radius * 0.78f);
        float puck_r = std::max(10.0f, radius * 0.085f);

        // Connecting line from center to puck
        p.setPen(QPen(QColor(0, 230, 255, 120), 1.5, Qt::SolidLine));
        p.drawLine(QPointF(center_x, center_y), QPointF(puck_x, puck_y));

        // Outer glow
        QRadialGradient puck_glow(puck_x, puck_y, puck_r * 2.2f);
        puck_glow.setColorAt(0.0f, QColor(0, 230, 255, 180));
        puck_glow.setColorAt(0.5f, QColor(0, 180, 255, 60));
        puck_glow.setColorAt(1.0f, QColor(0, 180, 255, 0));
        p.setBrush(puck_glow);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(puck_x, puck_y), puck_r * 2.2f, puck_r * 2.2f);

        // Main puck body
        QLinearGradient puck_body(puck_x - puck_r, puck_y - puck_r, puck_x + puck_r, puck_y + puck_r);
        puck_body.setColorAt(0.0f, QColor(240, 250, 255));
        puck_body.setColorAt(0.4f, QColor(0, 215, 255));
        puck_body.setColorAt(1.0f, QColor(0, 110, 200));

        p.setBrush(puck_body);
        p.setPen(QPen(QColor(255, 255, 255, 220), 1.5));
        p.drawEllipse(QPointF(puck_x, puck_y), puck_r, puck_r);

        // Center dot in puck
        p.setBrush(QColor(255, 255, 255));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(puck_x, puck_y), 2.5, 2.5);
    }
}

void CircularSpeakerWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        float center_x = width() * 0.5f;
        float center_y = height() * 0.5f;
        float radius = std::min(width(), height()) * 0.44f;
        float node_r = std::max(11.0f, radius * 0.095f);

        // Check if any speaker node was clicked (Solo preview)
        for (size_t i = 0; i < 8; ++i) {
            const auto& sp = SPEAKER_LAYOUT[i];
            if (active_channels_ <= 6 && (sp.ch == ChannelIndex::SideLeft || sp.ch == ChannelIndex::SideRight)) {
                continue;
            }
            float sp_x = center_x + sp.nx * (radius * 0.82f);
            float sp_y = center_y - sp.ny * (radius * 0.82f);
            float dist = std::hypot(event->position().x() - sp_x, event->position().y() - sp_y);
            if (dist <= node_r * 1.5f) {
                int ch_idx = static_cast<int>(sp.ch);
                // If this channel was already latched in solo, single-click unlatches and releases it!
                if (solo_latched_ && solo_active_channel_ == ch_idx) {
                    solo_latched_ = false;
                    solo_active_channel_ = -1;
                    update();
                    emit SoloChannelReleased();
                    return;
                }

                // Regular press down
                solo_latched_ = false;
                solo_active_channel_ = ch_idx;
                update();
                emit SoloChannelPressed(sp.ch);
                return;
            }
        }

        // If clicked elsewhere while solo was latched, unlatch and restore audio
        if (solo_latched_) {
            solo_latched_ = false;
            solo_active_channel_ = -1;
            update();
            emit SoloChannelReleased();
        }

        if (interactive_) {
            // Only start pan drag if click is INSIDE the circle boundary
            float dx = event->position().x() - center_x;
            float dy = event->position().y() - center_y;
            if (std::hypot(dx, dy) <= radius) {
                dragging_ = true;
                setCursor(Qt::ClosedHandCursor);
                UpdatePanFromMouse(event->position());
            }
        }
    }
}

void CircularSpeakerWidget::mouseMoveEvent(QMouseEvent* event) {
    if (solo_active_channel_ != -1 && !solo_latched_) {
        return;
    }
    if (!interactive_) return;
    if (dragging_) {
        UpdatePanFromMouse(event->position());
    } else {
        float center_x = width() * 0.5f;
        float center_y = height() * 0.5f;
        float radius = std::min(width(), height()) * 0.44f;
        float node_r = std::max(11.0f, radius * 0.095f);

        // Check if hovering speaker node
        bool on_speaker = false;
        for (size_t i = 0; i < 8; ++i) {
            const auto& sp = SPEAKER_LAYOUT[i];
            if (active_channels_ <= 6 && (sp.ch == ChannelIndex::SideLeft || sp.ch == ChannelIndex::SideRight)) {
                continue;
            }
            float sp_x = center_x + sp.nx * (radius * 0.82f);
            float sp_y = center_y - sp.ny * (radius * 0.82f);
            if (std::hypot(event->position().x() - sp_x, event->position().y() - sp_y) <= node_r * 1.5f) {
                on_speaker = true;
                break;
            }
        }

        if (on_speaker) {
            setCursor(Qt::PointingHandCursor);
            return;
        }

        // Check hover over puck
        float puck_x = center_x + pan_x_ * (radius * 0.78f);
        float puck_y = center_y - pan_y_ * (radius * 0.78f);
        float dist = std::hypot(event->position().x() - puck_x, event->position().y() - puck_y);
        if (dist <= radius * 0.15f) {
            setCursor(Qt::PointingHandCursor);
        } else {
            setCursor(Qt::CrossCursor);
        }
    }
}

void CircularSpeakerWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (solo_active_channel_ != -1 && !solo_latched_) {
            solo_active_channel_ = -1;
            update();
            emit SoloChannelReleased();
        }
        if (dragging_) {
            dragging_ = false;
            setCursor(Qt::ArrowCursor);
        }
    }
}

void CircularSpeakerWidget::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    if (solo_active_channel_ != -1 && !solo_latched_) {
        solo_active_channel_ = -1;
        update();
        emit SoloChannelReleased();
    }
}

void CircularSpeakerWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        float center_x = width() * 0.5f;
        float center_y = height() * 0.5f;
        float radius = std::min(width(), height()) * 0.44f;
        float node_r = std::max(11.0f, radius * 0.095f);

        // Check if double-clicked on any speaker node -> Lock/Latch Solo!
        for (size_t i = 0; i < 8; ++i) {
            const auto& sp = SPEAKER_LAYOUT[i];
            if (active_channels_ <= 6 && (sp.ch == ChannelIndex::SideLeft || sp.ch == ChannelIndex::SideRight)) {
                continue;
            }
            float sp_x = center_x + sp.nx * (radius * 0.82f);
            float sp_y = center_y - sp.ny * (radius * 0.82f);
            float dist = std::hypot(event->position().x() - sp_x, event->position().y() - sp_y);
            if (dist <= node_r * 1.5f) {
                solo_latched_ = true;
                solo_active_channel_ = static_cast<int>(sp.ch);
                update();
                emit SoloChannelPressed(sp.ch);
                return;
            }
        }

        if (interactive_) {
            // Reset pan to center
            pan_x_ = 0.0f;
            pan_y_ = 0.0f;
            update();
            emit PanChanged(pan_x_, pan_y_);
        }
    }
}

void CircularSpeakerWidget::UpdatePanFromMouse(const QPointF& pos) {
    float center_x = width() * 0.5f;
    float center_y = height() * 0.5f;
    float radius = std::min(width(), height()) * 0.44f * 0.78f;
    if (radius <= 1.0f) return;

    float dx = (pos.x() - center_x) / radius;
    float dy = -(pos.y() - center_y) / radius; // inverted y: up is positive

    // Clamping to unit circle or box
    float dist = std::hypot(dx, dy);
    if (dist > 1.0f) {
        dx /= dist;
        dy /= dist;
    }

    pan_x_ = std::clamp(dx, -1.0f, 1.0f);
    pan_y_ = std::clamp(dy, -1.0f, 1.0f);

    update();
    emit PanChanged(pan_x_, pan_y_);
}

} // namespace AeriaSurround
