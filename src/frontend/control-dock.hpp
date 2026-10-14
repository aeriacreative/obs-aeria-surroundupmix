#pragma once

#include <QDockWidget>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSplitter>
#include <memory>
#include <vector>
#include "circular-speaker-widget.hpp"
#include "channel-strip-widget.hpp"
#include "../state/filter-state.hpp"

namespace AeriaSurround {

// Unified "Sorround Control" dock — one dock for all filters.
// A dropdown in the header lets the user switch between registered filter states.
// Each filter keeps its own settings; switching the dropdown loads that filter's state.
class ControlDockWidget : public QDockWidget {
    Q_OBJECT

public:
    explicit ControlDockWidget(QWidget* parent = nullptr);
    ~ControlDockWidget() override;

    // Called by DockManager when filters are created / destroyed
    void AddFilterState(FilterState* state);
    void RemoveFilterState(FilterState* state);

    // Show this dock and focus on the specified filter in the dropdown
    void SetActiveFilter(FilterState* state);

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void OnMeterTimer();
    void OnSourceComboChanged(int index);
    void OnPanChanged(float x, float y);
    void OnGainChanged(ChannelIndex ch, float db);
    void OnToggleMeter(bool checked);
    void OnModeChanged(int index);
    void OnJumpToFilter(); ///< Opens the active filter's parent source filters dialog

private:
    // Registered filter states (ordered for combo)
    std::vector<FilterState*> filter_list_;
    FilterState* active_state_ = nullptr;

    QTimer* meter_timer_ = nullptr;
    bool meter_enabled_       = true;
    bool updating_from_state_ = false;

    // Header UI
    QComboBox*   source_combo_ = nullptr; ///< Dropdown: which filter to control
    QPushButton* meter_btn_    = nullptr;
    QPushButton* jump_btn_     = nullptr; ///< Opens active filter's source filters dialog

    // Main controls
    QSplitter*             splitter_  = nullptr; ///< Draggable divider: panner ↕ strips
    QComboBox*             mode_combo_ = nullptr;
    CircularSpeakerWidget* panner_     = nullptr;
    QLabel*                pan_label_  = nullptr;
    std::vector<ChannelStripWidget*> strips_;

    // Placeholder shown when no filters are registered
    QLabel*  no_filter_label_ = nullptr;
    QWidget* controls_widget_ = nullptr; ///< Hidden when no filters

    QLabel* credit_label_ = nullptr;

    void BuildUI();
    void LoadFromState();
    void UpdateChannelVisibility();
    void SyncCombo();   ///< Rebuild combo items from filter_list_

    QString FilterDisplayName(FilterState* s) const;
};

} // namespace AeriaSurround
