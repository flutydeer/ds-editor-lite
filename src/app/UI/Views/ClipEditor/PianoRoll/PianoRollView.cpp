#include "PianoRollView.h"

#include "PianoKeyboardView.h"
#include "PianoRollGraphicsScene.h"
#include "PianoRollGraphicsView.h"
#include "PianoRollContextMenuController.h"
#include "PianoRollRhiWidget.h"
#include "PhonemeView.h"
#include "Controller/ClipController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Views/Common/TimeGraphicsView.h"
#include "UI/Views/Common/TimelineView.h"
#include "UI/Views/Common/EditorShortcutUtils.h"

#include <QLabel>
#include <QEvent>
#include <QHideEvent>
#include <QRectF>
#include <QShowEvent>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

PianoRollView::PianoRollView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setMinimumHeight(128);
    m_contextMenuController = new PianoRollContextMenuController(this);

    const auto useRhi = appOptions->developer()->editorRenderBackend ==
                        DeveloperOption::EditorRenderBackend::RhiExperimental;
    if (useRhi) {
        m_rhiView = new PianoRollRhiWidget(this);
        m_editorWidget = m_rhiView;
    } else {
        createLegacyBackend();
        m_editorWidget = m_graphicsView;
    }

    m_timelineView = new TimelineView;
    m_timelineView->setObjectName("pianoRollTimelineView");
    m_timelineView->setTimeRange(startTick(), endTick());
    m_timelineView->setPixelsPerQuarterNote(pixelsPerQuarterNote);
    m_timelineView->setQuantize(appStatus->pianoRollQuantize);
    connect(appStatus, &AppStatus::pianoRollQuantizeChanged, m_timelineView,
            &TimelineView::setQuantize);
    m_timelineView->setFixedHeight(timelineViewHeight);

    m_keyboardView = new PianoKeyboardView;
    m_keyboardView->setKeyRange(topKeyIndex(), bottomKeyIndex());

    m_phonemeView = new PhonemeView;
    m_phonemeView->setTimeRange(startTick(), endTick());
    m_phonemeView->setFixedHeight(40);
    m_phonemeView->setVisible(false);
    connect(m_phonemeView, &PhonemeView::wheelHorScale, this, &PianoRollView::onWheelHorScale);
    connect(m_phonemeView, &PhonemeView::wheelHorScroll, this, &PianoRollView::onWheelHorScroll);

    m_lbTip = new QLabel(tr("Select a singing clip to edit"));
    m_lbTip->setObjectName("lbNullClipTip");
    m_lbTip->setAlignment(Qt::AlignCenter);

    const auto topLeftSpacing = new QWidget();
    topLeftSpacing->setObjectName("pianoRollTopLeftSpacing");
    topLeftSpacing->setMinimumWidth(0);
    topLeftSpacing->setFixedHeight(timelineViewHeight);

    const auto bottomLeftSpacing = new QWidget();
    bottomLeftSpacing->setObjectName("pianoRollBottomLeftSpacing");
    bottomLeftSpacing->setMinimumWidth(0);
    bottomLeftSpacing->setFixedHeight(m_phonemeView->height());

    const auto pianoKeyboardLayout = new QVBoxLayout;
    pianoKeyboardLayout->setContentsMargins(0, 0, 0, 0);
    pianoKeyboardLayout->setSpacing(0);
    pianoKeyboardLayout->addWidget(topLeftSpacing);
    pianoKeyboardLayout->addWidget(m_keyboardView);
    pianoKeyboardLayout->addWidget(bottomLeftSpacing);

    m_rightLayout = new QVBoxLayout;
    m_rightLayout->setContentsMargins(0, 0, 0, 0);
    m_rightLayout->setSpacing(0);
    m_rightLayout->addWidget(m_timelineView);
    m_rightLayout->addWidget(m_editorWidget);
    m_rightLayout->addWidget(m_phonemeView);

    const auto layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(pianoKeyboardLayout);
    layout->addLayout(m_rightLayout, 1);
    layout->addWidget(m_lbTip, 1);
    setLayout(layout);

    connect(m_timelineView, &TimelineView::wheelHorScale, this, &PianoRollView::onWheelHorScale);
    connect(m_keyboardView, &PianoKeyboardView::wheelScroll, this, [this](QWheelEvent *event) {
        if (m_rhiView)
            m_rhiView->onWheelVerScale(event);
        else
            m_graphicsView->onWheelVerScale(event);
    });
    if (m_rhiView)
        connectRhiBackend();
    else
        connectLegacyBackend();
    registerEditorShortcuts();
    // 底部面板折叠（docked 模式为 splitter 高度归零，不触发 hideEvent）时
    // 使轨道侧叠加层失效；展开且本视图可见时恢复
    connect(appStatus, &AppStatus::bottomPanelCollapseStateChanged, this,
            [this](const bool collapsed) {
                if (collapsed)
                    appStatus->pianoRollVisibleRect = QRectF();
                else if (isVisible())
                    updatePianoRollVisibleRect();
            });
}

void PianoRollView::registerEditorShortcuts() {
    using EditorShortcutUtils::add;
    add(this, QKeySequence::Cut, m_contextMenuController,
        &PianoRollContextMenuController::cutSelection);
    add(this, QKeySequence::Copy, m_contextMenuController,
        &PianoRollContextMenuController::copySelection);
    add(this, QKeySequence::Paste, m_contextMenuController,
        &PianoRollContextMenuController::pasteSelection);
    add(this, QKeySequence::SelectAll, m_contextMenuController,
        &PianoRollContextMenuController::selectAll);
    const auto remove = [this] {
        if (m_editMode == EditPitchAnchor) {
            if (m_rhiView)
                m_rhiView->deleteSelectedAnchors();
            else if (m_graphicsView)
                m_graphicsView->deleteSelectedAnchors();
        } else {
            m_contextMenuController->deleteSelection();
        }
    };
    add(this, QKeySequence::Delete, this, remove);
    add(this, QKeySequence(Qt::Key_Backspace), this, remove);
}

void PianoRollView::createLegacyBackend() {
    m_scene = new PianoRollGraphicsScene;
    m_graphicsView = new PianoRollGraphicsView(m_scene, this);
}

void PianoRollView::connectLegacyBackend() {
    connect(m_graphicsView, &TimeGraphicsView::timeRangeChanged, m_timelineView,
            &TimelineView::setTimeRange);
    connect(m_graphicsView, &TimeGraphicsView::timeRangeChanged, m_phonemeView,
            &PhonemeView::setTimeRange);
    connect(m_graphicsView, &PianoRollGraphicsView::keyRangeChanged, m_keyboardView,
            &PianoKeyboardView::setKeyRange);
    connect(m_graphicsView, &PianoRollGraphicsView::keyHovered, m_keyboardView,
            &PianoKeyboardView::setHoveredKeyIndex);
    connect(m_graphicsView, &PianoRollGraphicsView::keyHoverCleared, m_keyboardView,
            [this] { m_keyboardView->setHoveredKeyIndex(-1); });
    connect(m_graphicsView, &TimeGraphicsView::scaleChanged, this,
            [this](const double horizontal, const double vertical) {
                emit scaleChanged(horizontal, vertical);
            });
    connect(m_graphicsView->horizontalScrollBar(), &QScrollBar::valueChanged, this,
            &PianoRollView::horizontalBarValueChanged);
    connect(m_graphicsView, &PianoRollGraphicsView::contextMenuRequested, this,
            [this](const PianoRollMenuContext &context) {
                m_contextMenuController->showMenu(context, m_clip, m_graphicsView, m_graphicsView);
            });
    connect(m_graphicsView, &TimeGraphicsView::autoPageTurnAvailabilityChanged, this,
            &PianoRollView::updateAutoPageTurnButtonView);
    connect(m_graphicsView, &TimeGraphicsView::timeRangeChanged, this,
            [this](double, double) { updatePianoRollVisibleRect(); });
    connect(m_graphicsView, &PianoRollGraphicsView::keyRangeChanged, this,
            [this](double, double) { updatePianoRollVisibleRect(); });
    m_graphicsView->setAutoTurnPage(appStatus->pianoRollAutoPageTurnEnabled);
}

void PianoRollView::connectRhiBackend() {
    connect(m_rhiView, &PianoRollRhiWidget::timeRangeChanged, m_timelineView,
            &TimelineView::setTimeRange);
    connect(m_rhiView, &PianoRollRhiWidget::timeRangeChanged, m_phonemeView,
            &PhonemeView::setTimeRange);
    connect(m_rhiView, &PianoRollRhiWidget::keyRangeChanged, m_keyboardView,
            &PianoKeyboardView::setKeyRange);
    connect(m_rhiView, &PianoRollRhiWidget::keyHovered, m_keyboardView,
            &PianoKeyboardView::setHoveredKeyIndex);
    connect(m_rhiView, &PianoRollRhiWidget::keyHoverCleared, m_keyboardView,
            [this] { m_keyboardView->setHoveredKeyIndex(-1); });
    connect(m_rhiView, &PianoRollRhiWidget::scaleChanged, this, &PianoRollView::scaleChanged);
    connect(m_rhiView, &PianoRollRhiWidget::horizontalBarValueChanged, this,
            &PianoRollView::horizontalBarValueChanged);
    connect(m_rhiView, &PianoRollRhiWidget::backendUnavailable, this,
            &PianoRollView::fallbackToLegacy);
    connect(m_rhiView, &PianoRollRhiWidget::contextMenuRequested, this,
            [this](const PianoRollMenuContext &context) {
                m_contextMenuController->showMenu(context, m_clip, m_rhiView, m_rhiView);
            });
    connect(m_rhiView, &PianoRollRhiWidget::autoPageTurnAvailabilityChanged, this,
            &PianoRollView::updateAutoPageTurnButtonView);
    connect(m_rhiView, &PianoRollRhiWidget::timeRangeChanged, this,
            [this](double, double) { updatePianoRollVisibleRect(); });
    connect(m_rhiView, &PianoRollRhiWidget::keyRangeChanged, this,
            [this](double, double) { updatePianoRollVisibleRect(); });
    m_rhiView->setAutoPageTurn(appStatus->pianoRollAutoPageTurnEnabled);
}

void PianoRollView::fallbackToLegacy() {
    if (!m_rhiView || m_graphicsView)
        return;
    const auto state = m_rhiView->viewState();
    auto *failedView = m_rhiView;
    m_rhiView = nullptr;
    createLegacyBackend();
    m_editorWidget = m_graphicsView;
    m_rightLayout->replaceWidget(failedView, m_graphicsView);
    failedView->hide();
    failedView->deleteLater();
    connectLegacyBackend();
    m_graphicsView->setDataContext(m_clip);
    onEditModeChanged(m_editMode);
    QTimer::singleShot(0, this, [this, state] {
        setViewScale(state.horizontalScale, state.verticalScale);
        centerAt(state.centerTick, state.centerKeyIndex);
        updatePianoRollVisibleRect();
    });
    qWarning() << "[PianoRollRhi] backend failed; restored Legacy view";
}

void PianoRollView::setDataContext(SingingClip *clip) const {
    m_clip = clip;
    if (m_rhiView)
        m_rhiView->setDataContext(clip);
    else
        m_graphicsView->setDataContext(clip);
    m_phonemeView->setDataContext(clip);
    m_timelineView->setDataContext(clip);

    const bool notNull = clip != nullptr;
    m_timelineView->setVisible(notNull);
    m_editorWidget->setVisible(notNull);
    m_phonemeView->setVisible(notNull);
    m_keyboardView->setVisible(notNull);
    m_lbTip->setVisible(!notNull);

    if (clip)
        updatePianoRollVisibleRect();
}

void PianoRollView::onEditModeChanged(const PianoRollEditMode mode) const {
    m_editMode = mode;
    if (m_rhiView)
        m_rhiView->setEditMode(mode);
    else
        m_graphicsView->setEditMode(mode);
    if (EditorViewGlobal::isPitchEditMode(mode))
        clipController->selectNotes({}, true);
}

void PianoRollView::setTrackColorIndex(int index) const {
    m_trackColorIndex = index;
    m_keyboardView->setTrackColorIndex(index);
    if (m_rhiView)
        m_rhiView->setTrackColorIndex(index);
}

PianoRollViewState PianoRollView::viewState() const {
    if (m_rhiView)
        return m_rhiView->viewState();
    return {(m_graphicsView->startTick() + m_graphicsView->endTick()) * 0.5,
            m_graphicsView->centerKeyIndex(), m_graphicsView->scaleX(), m_graphicsView->scaleY(),
            m_editMode};
}

bool PianoRollView::centerAt(const double tick, const double keyIndex) const {
    if (m_rhiView)
        return m_rhiView->centerAt(tick, keyIndex);
    m_graphicsView->stopViewportAnimations();
    m_graphicsView->setViewportCenterAt(tick, keyIndex, false);
    return true;
}

bool PianoRollView::setViewScale(const double horizontalScale, const double verticalScale) const {
    if (m_rhiView)
        return m_rhiView->setViewScale(horizontalScale, verticalScale);
    return m_graphicsView->setViewportScale(horizontalScale, verticalScale);
}

HistoryFocusVisibility PianoRollView::focusVisibility(const HistoryFocus &focus) const {
    return m_rhiView ? m_rhiView->focusVisibility(focus) : m_graphicsView->focusVisibility(focus);
}

bool PianoRollView::revealFocus(const HistoryFocus &focus, const bool animated) const {
    return m_rhiView ? m_rhiView->revealFocus(focus, animated)
                     : m_graphicsView->revealFocus(focus, animated);
}

double PianoRollView::scaleX() const {
    return m_rhiView ? m_rhiView->scaleX() : m_graphicsView->scaleX();
}

int PianoRollView::horizontalBarValue() const {
    return m_rhiView ? m_rhiView->horizontalBarValue() : m_graphicsView->horizontalBarValue();
}

void PianoRollView::onWheelHorScale(QWheelEvent *event) const {
    if (m_rhiView)
        m_rhiView->onWheelHorScale(event);
    else
        m_graphicsView->onWheelHorScale(event);
}

void PianoRollView::onWheelHorScroll(QWheelEvent *event) const {
    if (m_rhiView)
        m_rhiView->onWheelHorScroll(event);
    else
        m_graphicsView->onWheelHorScroll(event);
}

void PianoRollView::setHorizontalBarValue(const int value) const {
    if (m_rhiView)
        m_rhiView->setHorizontalBarValue(value);
    else
        m_graphicsView->setHorizontalBarValue(value);
}

void PianoRollView::setPlaybackPosition(const double tick) const {
    if (m_rhiView)
        m_rhiView->setPlaybackPosition(tick);
    else
        m_graphicsView->setPlaybackPosition(tick);
}

void PianoRollView::setLastPlaybackPosition(const double tick) const {
    if (m_rhiView)
        m_rhiView->setLastPlaybackPosition(tick);
    else
        m_graphicsView->setLastPlaybackPosition(tick);
}

double PianoRollView::startTick() const {
    return m_rhiView ? m_rhiView->startTick() : m_graphicsView->startTick();
}

double PianoRollView::endTick() const {
    return m_rhiView ? m_rhiView->endTick() : m_graphicsView->endTick();
}

double PianoRollView::topKeyIndex() const {
    return m_rhiView ? m_rhiView->topKeyIndex() : m_graphicsView->topKeyIndex();
}

double PianoRollView::bottomKeyIndex() const {
    return m_rhiView ? m_rhiView->bottomKeyIndex() : m_graphicsView->bottomKeyIndex();
}

void PianoRollView::updatePianoRollVisibleRect() const {
    if (!m_clip) {
        appStatus->pianoRollVisibleRect = QRectF();
        return;
    }
    const double start = startTick();
    const double end = endTick();
    const double top = topKeyIndex();
    const double bottom = bottomKeyIndex();
    // x 轴存相对 active clip 的局部 tick；轨道侧叠加层用 clip 当前 start() 平移，
    // 拖动 clip（view start 实时变化、model start 未提交）时叠加层随之跟随
    appStatus->pianoRollVisibleRect =
        QRectF(start - m_clip->start(), bottom, end - start, top - bottom);
}

void PianoRollView::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        m_lbTip->setText(tr("Select a singing clip to edit"));
}

void PianoRollView::hideEvent(QHideEvent *event) {
    // 钢琴卷帘不可见（底部面板折叠 / 切换页面）时，轨道侧叠加层随之失效
    appStatus->pianoRollVisibleRect = QRectF();
    QWidget::hideEvent(event);
}

void PianoRollView::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (appStatus->bottomPanelCollapsed) {
        appStatus->pianoRollVisibleRect = QRectF();
        return;
    }
    updatePianoRollVisibleRect();
}

void PianoRollView::updateAutoPageTurnButtonView(const bool available) {
    appStatus->pianoRollAutoPageTurnAvailable = available;
}
