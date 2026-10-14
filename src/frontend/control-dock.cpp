#include "control-dock.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QApplication>
#include <QSignalBlocker>
#include <algorithm>
#include <obs-module.h>
#include <obs-frontend-api.h>

namespace AeriaSurround {

// Channel strip definitions (order matches UpmixParams::channel_gain_db indices)
static const struct {
    ChannelIndex ch;
    const char*  label;
    QColor       color;
} STRIP_DEFS[8] = {
    { ChannelIndex::FrontLeft,  "L",   QColor(0, 210, 255) },
    { ChannelIndex::FrontRight, "R",   QColor(0, 210, 255) },
    { ChannelIndex::Center,     "C",   QColor(0, 255, 180) },
    { ChannelIndex::LFE,        "LFE", QColor(255, 140, 0) },
    { ChannelIndex::RearLeft,   "RL",  QColor(140, 120, 255) },
    { ChannelIndex::RearRight,  "RR",  QColor(140, 120, 255) },
    { ChannelIndex::SideLeft,   "SL",  QColor(100, 180, 255) },
    { ChannelIndex::SideRight,  "SR",  QColor(100, 180, 255) },
};

// ─── Constructor / Destructor ────────────────────────────────────────────────

ControlDockWidget::ControlDockWidget(QWidget* parent)
    : QDockWidget(parent)
{
    setObjectName("obs_surround_control_dock");
    setWindowTitle("Sorround Control");

    BuildUI();

    // Start with no filter — show placeholder
    if (no_filter_label_)  no_filter_label_->setVisible(true);
    if (controls_widget_)  controls_widget_->setVisible(false);
    SyncCombo();

    meter_timer_ = new QTimer(this);
    connect(meter_timer_, &QTimer::timeout, this, &ControlDockWidget::OnMeterTimer);
    meter_timer_->start(33); // ~30 fps
}

ControlDockWidget::~ControlDockWidget() {
    // Clear callbacks on all registered filters to avoid dangling references
    for (FilterState* s : filter_list_) {
        if (s) s->SetChangeCallback(nullptr);
    }
}

// ─── Filter Registry ─────────────────────────────────────────────────────────

QString ControlDockWidget::FilterDisplayName(FilterState* s) const {
    if (!s) return "(Unknown)";
    return QString::fromStdString(s->GetDisplayName());
}

void ControlDockWidget::AddFilterState(FilterState* state) {
    if (!state) return;
    // Guard against duplicates
    for (auto* s : filter_list_) { if (s == state) return; }

    filter_list_.push_back(state);
    SyncCombo();

    // Make it active if it's the first one registered
    if (filter_list_.size() == 1) {
        SetActiveFilter(state);
    }
}

void ControlDockWidget::RemoveFilterState(FilterState* state) {
    if (!state) return;

    bool was_active = (active_state_ == state);
    if (was_active) {
        state->SetChangeCallback(nullptr);
        active_state_ = nullptr;
    }

    filter_list_.erase(
        std::remove(filter_list_.begin(), filter_list_.end(), state),
        filter_list_.end()
    );
    SyncCombo();

    if (was_active) {
        if (!filter_list_.empty()) {
            // Auto-switch to the first remaining filter
            SetActiveFilter(filter_list_.front());
        } else {
            // No filters left — show placeholder
            if (no_filter_label_) no_filter_label_->setVisible(true);
            if (controls_widget_) controls_widget_->setVisible(false);
        }
    }
}

void ControlDockWidget::SetActiveFilter(FilterState* state) {
    if (active_state_ == state) {
        // Same filter: just sync combo selection and return
        SyncCombo();
        return;
    }

    // Detach old state
    if (active_state_) {
        active_state_->SetChangeCallback(nullptr);
        if (isVisible()) active_state_->SetDockVisible(false);
    }

    active_state_ = state;

    if (!state) {
        if (no_filter_label_) no_filter_label_->setVisible(true);
        if (controls_widget_) controls_widget_->setVisible(false);
        SyncCombo();
        return;
    }

    // Attach new state: receive param changes from DSP/filter
    state->SetChangeCallback([this](const UpmixParams&) {
        QMetaObject::invokeMethod(this, [this]() {
            LoadFromState();
        }, Qt::QueuedConnection);
    });

    if (isVisible()) state->SetDockVisible(true);

    if (no_filter_label_) no_filter_label_->setVisible(false);
    if (controls_widget_) controls_widget_->setVisible(true);

    SyncCombo();
    LoadFromState();
}

void ControlDockWidget::SyncCombo() {
    if (!source_combo_) return;
    QSignalBlocker blocker(source_combo_);
    source_combo_->clear();

    if (filter_list_.empty()) {
        source_combo_->addItem("(No Sorround Upmix filter found)");
        source_combo_->setEnabled(false);
        return;
    }

    source_combo_->setEnabled(true);
    int active_idx = 0;
    for (int i = 0; i < (int)filter_list_.size(); ++i) {
        source_combo_->addItem(FilterDisplayName(filter_list_[i]));
        if (filter_list_[i] == active_state_) active_idx = i;
    }
    source_combo_->setCurrentIndex(active_idx);
}

// ─── Dock Visibility ─────────────────────────────────────────────────────────

void ControlDockWidget::showEvent(QShowEvent* event) {
    QDockWidget::showEvent(event);
    if (active_state_) active_state_->SetDockVisible(true);
}

void ControlDockWidget::closeEvent(QCloseEvent* event) {
    if (active_state_) active_state_->SetDockVisible(false);
    QDockWidget::closeEvent(event);
}

// ─── UI Construction ─────────────────────────────────────────────────────────

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

    // ── Source selector row ───────────────────────────────────────────────────
    auto* source_row = new QHBoxLayout();
    source_row->setSpacing(6);

    auto* source_lbl = new QLabel("Source:", container);
    source_lbl->setStyleSheet("font-size: 11px; color: #8c96ab; min-width: 44px;");
    source_row->addWidget(source_lbl);

    source_combo_ = new QComboBox(container);
    source_combo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    source_combo_->setMinimumContentsLength(4);
    source_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    connect(source_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ControlDockWidget::OnSourceComboChanged);
    source_row->addWidget(source_combo_, 1);

    meter_btn_ = new QPushButton("Meter", container);
    meter_btn_->setCheckable(true);
    meter_btn_->setChecked(true);
    meter_btn_->setToolTip("Toggle level meters");
    meter_btn_->setFixedWidth(52);
    connect(meter_btn_, &QPushButton::toggled, this, &ControlDockWidget::OnToggleMeter);
    source_row->addWidget(meter_btn_);

    jump_btn_ = new QPushButton("➜", container);
    jump_btn_->setToolTip("Jump to filter — Open the source filter dialog for the selected filter");
    jump_btn_->setFixedWidth(32);
    jump_btn_->setStyleSheet(
        "QPushButton { font-size: 14px; padding: 2px 4px; }"
        "QPushButton:hover { background: #1a4a6a; border-color: #00d2ff; color: #00d2ff; }"
    );
    connect(jump_btn_, &QPushButton::clicked, this, &ControlDockWidget::OnJumpToFilter);
    source_row->addWidget(jump_btn_);

    root_layout->addLayout(source_row);

    // ── Separator ─────────────────────────────────────────────────────────────
    auto* sep0 = new QFrame(container);
    sep0->setFrameShape(QFrame::HLine);
    sep0->setStyleSheet("color: #242833;");
    root_layout->addWidget(sep0);

    // ── Placeholder: no filters yet ───────────────────────────────────────────
    no_filter_label_ = new QLabel(
        "No Sorround Upmix filter found.\n\n"
        "Add a Sorround Upmix filter to any audio source,\n"
        "then click \"Open Sorround Control\" in the filter\n"
        "properties panel.",
        container);
    no_filter_label_->setAlignment(Qt::AlignCenter);
    no_filter_label_->setStyleSheet("color: #4a5368; font-size: 12px; padding: 28px 16px;");
    no_filter_label_->setWordWrap(true);
    root_layout->addWidget(no_filter_label_, 1);

    // ── Main controls (hidden until a filter is active) ───────────────────────
    controls_widget_ = new QWidget(container);
    auto* ctrl = new QVBoxLayout(controls_widget_);
    ctrl->setContentsMargins(0, 0, 0, 0);
    ctrl->setSpacing(6);

    // Mode selector
    auto* mode_row = new QHBoxLayout();
    mode_row->setSpacing(6);
    mode_row->addWidget(new QLabel("Mode:", controls_widget_));
    mode_combo_ = new QComboBox(controls_widget_);
    mode_combo_->addItem("Auto (Detect OBS Output)");           // UpmixMode::Auto = 0
    mode_combo_->addItem("Force 5.1 Surround");                 // UpmixMode::Force5Point1 = 1
    mode_combo_->addItem("Force 7.1 Surround");                 // UpmixMode::Force7Point1 = 2
    mode_combo_->addItem("Mono Surround (Speaker Balance Test)");// UpmixMode::MonoSurround = 3
    mode_combo_->addItem("Direct Pass-Through");                 // UpmixMode::Direct = 4
    connect(mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ControlDockWidget::OnModeChanged);
    mode_row->addWidget(mode_combo_, 1);
    ctrl->addLayout(mode_row);

    // ── Splitter: circular panner (top) ↕ channel strips (bottom) ───────────
    // The handle is draggable so the user can give more height to either section.
    splitter_ = new QSplitter(Qt::Vertical, controls_widget_);
    splitter_->setChildrenCollapsible(false);
    splitter_->setStyleSheet(
        "QSplitter::handle:vertical {"
        "  background: #2a2f3d;"
        "  height: 5px;"
        "  border-top: 1px solid #3a4155;"
        "  border-bottom: 1px solid #3a4155;"
        "}"
        "QSplitter::handle:vertical:hover {"
        "  background: #00d2ff;"
        "}"
    );

    // Top pane: circular panner + pan coordinate label
    auto* panner_pane = new QWidget(splitter_);
    auto* panner_pane_l = new QVBoxLayout(panner_pane);
    panner_pane_l->setContentsMargins(0, 0, 0, 0);
    panner_pane_l->setSpacing(3);

    panner_ = new CircularSpeakerWidget(true, panner_pane);
    panner_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(panner_, &CircularSpeakerWidget::PanChanged,
            this, &ControlDockWidget::OnPanChanged);
    connect(panner_, &CircularSpeakerWidget::SoloChannelPressed,
            this, [this](ChannelIndex ch) {
        if (active_state_) active_state_->SetSoloChannel(static_cast<int>(ch));
        bool latched = panner_->IsSoloLatched();
        for (auto* s : strips_)
            s->SetSoloHighlighted(s->GetChannel() == ch, latched);
    });
    connect(panner_, &CircularSpeakerWidget::SoloChannelReleased,
            this, [this]() {
        if (active_state_) active_state_->SetSoloChannel(-1);
        for (auto* s : strips_) s->UnlatchSolo();
    });
    panner_pane_l->addWidget(panner_, 1);

    pan_label_ = new QLabel("Pan: Center (0.00, 0.00) — Drag handle or double-click to center",
                            panner_pane);
    pan_label_->setAlignment(Qt::AlignCenter);
    pan_label_->setStyleSheet("color: #8c96ab; font-size: 11px;");
    pan_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    pan_label_->setMinimumWidth(0);
    panner_pane_l->addWidget(pan_label_);

    splitter_->addWidget(panner_pane);

    // Bottom pane: channel strips (gain sliders + meters per channel)
    auto* strips_w = new QWidget(splitter_);
    auto* strips_l = new QHBoxLayout(strips_w);
    strips_l->setContentsMargins(0, 4, 0, 0);
    strips_l->setSpacing(3);
    strips_.clear();
    for (size_t i = 0; i < 8; ++i) {
        auto* strip = new ChannelStripWidget(
            STRIP_DEFS[i].ch, STRIP_DEFS[i].label, STRIP_DEFS[i].color, strips_w);
        connect(strip, &ChannelStripWidget::GainChanged,
                this, &ControlDockWidget::OnGainChanged);
        connect(strip, &ChannelStripWidget::SoloPressed,
                this, [this](ChannelIndex ch) {
            if (active_state_) active_state_->SetSoloChannel(static_cast<int>(ch));
            panner_->SetSoloState(static_cast<int>(ch), true);
        });
        connect(strip, &ChannelStripWidget::SoloReleased,
                this, [this]() {
            if (active_state_) active_state_->SetSoloChannel(-1);
            panner_->SetSoloState(-1, false);
        });
        strips_l->addWidget(strip);
        strips_.push_back(strip);
    }
    splitter_->addWidget(strips_w);

    // Initial ratio ~65% panner / 35% sliders
    splitter_->setStretchFactor(0, 2);
    splitter_->setStretchFactor(1, 1);

    ctrl->addWidget(splitter_, 1);

    root_layout->addWidget(controls_widget_, 1);

    // ── Separator + Footer ────────────────────────────────────────────────────
    auto* sep3 = new QFrame(container);
    sep3->setFrameShape(QFrame::HLine);
    sep3->setStyleSheet("color: #242833;");
    root_layout->addWidget(sep3);

    credit_label_ = new QLabel(
        "<a href=\"https://linktr.ee/aeriacreative\" "
        "style=\"color: #00d2ff; text-decoration: none;\">"
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

    // Set the container as dock widget — no QScrollArea so panner fills height
    setWidget(container);
    setMinimumWidth(200);
    setMinimumHeight(340);
}

// ─── State Sync ──────────────────────────────────────────────────────────────

void ControlDockWidget::UpdateChannelVisibility() {
    if (!active_state_) return;
    UpmixMode mode = active_state_->GetParams().mode;
    size_t active_ch = GetMasterOutputChannels(mode);
    for (auto* strip : strips_) {
        bool is_side = (strip->GetChannel() == ChannelIndex::SideLeft ||
                        strip->GetChannel() == ChannelIndex::SideRight);
        if (is_side) strip->setVisible(active_ch >= 8);
    }
}

void ControlDockWidget::LoadFromState() {
    if (!active_state_) return;
    updating_from_state_ = true;

    UpmixParams p = active_state_->GetParams();

    // Mode combo index matches UpmixMode enum value directly
    { QSignalBlocker b(mode_combo_); mode_combo_->setCurrentIndex(static_cast<int>(p.mode)); }

    panner_->SetPan(p.pan_x, p.pan_y);
    pan_label_->setText(
        QString("Pan: X = %1, Y = %2  (Double-click to center)")
            .arg(p.pan_x, 5, 'f', 2)
            .arg(p.pan_y, 5, 'f', 2));

    for (auto* strip : strips_) {
        size_t idx = static_cast<size_t>(strip->GetChannel());
        if (idx < 8) strip->SetGainDb(p.channel_gain_db[idx]);
    }

    UpdateChannelVisibility();
    updating_from_state_ = false;
}

// ─── Slots ───────────────────────────────────────────────────────────────────

void ControlDockWidget::OnMeterTimer() {
    if (!active_state_ || !meter_enabled_) return;

    UpmixMode mode = active_state_->GetParams().mode;
    size_t active_ch = GetMasterOutputChannels(mode);

    ChannelMeterData levels[METER_MAX_CHANNELS];
    active_state_->GetMeter().GetSnapshot(levels, METER_MAX_CHANNELS);

    panner_->SetChannelLevels(levels, active_ch);
    for (auto* strip : strips_) {
        size_t idx = static_cast<size_t>(strip->GetChannel());
        if (idx < METER_MAX_CHANNELS) strip->SetMeterData(levels[idx]);
    }
}

void ControlDockWidget::OnSourceComboChanged(int index) {
    if (updating_from_state_) return;
    if (index < 0 || index >= (int)filter_list_.size()) return;

    FilterState* new_state = filter_list_[index];
    if (new_state == active_state_) return;

    SetActiveFilter(new_state);
}

void ControlDockWidget::OnPanChanged(float x, float y) {
    if (updating_from_state_ || !active_state_) return;
    pan_label_->setText(
        QString("Pan: X = %1, Y = %2  (Double-click to center)")
            .arg(x, 5, 'f', 2)
            .arg(y, 5, 'f', 2));
    active_state_->SetPanning(x, y);
}

void ControlDockWidget::OnGainChanged(ChannelIndex ch, float db) {
    if (updating_from_state_ || !active_state_) return;
    active_state_->SetChannelGain(ch, db);
}

void ControlDockWidget::OnToggleMeter(bool checked) {
    meter_enabled_ = checked;
    meter_btn_->setText(checked ? "Meter" : "Paused");
    if (!checked) {
        ChannelMeterData zero[METER_MAX_CHANNELS];
        size_t ch = active_state_ ? GetMasterOutputChannels(active_state_->GetParams().mode) : 2;
        panner_->SetChannelLevels(zero, ch);
        for (auto* strip : strips_) strip->SetMeterData(ChannelMeterData());
    }
}

void ControlDockWidget::OnModeChanged(int index) {
    if (updating_from_state_ || !active_state_) return;
    active_state_->SetMode(static_cast<UpmixMode>(index));
    UpdateChannelVisibility();
}

void ControlDockWidget::OnJumpToFilter() {
    if (!active_state_) return;

    // Get the filter obs_source_t safely with an addref
    obs_source_t* filter_ctx = active_state_->GetFilterContext();
    if (!filter_ctx) return;

    obs_source_t* filter_ref = obs_source_get_ref(filter_ctx);
    if (!filter_ref) return;

    // Get the parent source that owns the filter
    obs_source_t* parent = obs_filter_get_parent(filter_ref);
    if (parent) {
        // Open the OBS filters dialog for the parent source
        obs_frontend_open_source_filters(parent);
    }

    obs_source_release(filter_ref);
}

} // namespace AeriaSurround
