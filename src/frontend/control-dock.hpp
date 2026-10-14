#pragma once

#include <QDockWidget>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <memory>
#include <vector>
#include "circular-speaker-widget.hpp"
#include "channel-strip-widget.hpp"
#include "../state/filter-state.hpp"

namespace AeriaSurround {

class ControlDockWidget : public QDockWidget {
    Q_OBJECT

public:
    explicit ControlDockWidget(FilterState* filter_state, QWidget* parent = nullptr);
    ~ControlDockWidget() override;

    const std::string& GetInstanceId() const { return instance_id_; }
    void DetachFilter();
    void RefreshTitle();

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void OnMeterTimer();
    void OnPanChanged(float x, float y);
    void OnGainChanged(ChannelIndex ch, float db);
    void OnToggleMeter(bool checked);
    void OnModeChanged(int index);
    void OnCreditClicked();

private:
    std::string instance_id_;
    FilterState* filter_state_ = nullptr;

    QTimer* meter_timer_ = nullptr;
    bool meter_enabled_ = true;
    bool updating_from_state_ = false;

    // UI elements
    QLabel* header_title_ = nullptr;
    QLabel* header_status_ = nullptr;
    QPushButton* meter_btn_ = nullptr;

    CircularSpeakerWidget* panner_ = nullptr;
    QLabel* pan_label_ = nullptr;

    QComboBox* mode_combo_ = nullptr;
    std::vector<ChannelStripWidget*> strips_;

    QLabel* credit_label_ = nullptr;

    void BuildUI();
    void LoadFromState();
    void UpdateChannelVisibility();
};

} // namespace AeriaSurround
