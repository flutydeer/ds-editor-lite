#include "LegacyPianoRollCanvasAdapter.h"

#include "NoteView.h"
#include "PianoRollGraphicsScene.h"
#include "PianoRollGraphicsView.h"
#include "Model/AppStatus/AppStatus.h"

#include <QGraphicsItem>
#include <QScrollBar>
#include <QSignalBlocker>

LegacyPianoRollCanvasAdapter::LegacyPianoRollCanvasAdapter(QObject *parent)
    : IPianoRollCanvas(parent), m_scene(new PianoRollGraphicsScene),
      m_view(new PianoRollGraphicsView(m_scene)) {
    m_scene->setParent(this);
    connect(m_view, &PianoRollGraphicsView::scaleChanged, this, &IPianoRollCanvas::scaleChanged);
    connect(m_view, &PianoRollGraphicsView::timeRangeChanged, this,
            &IPianoRollCanvas::timeRangeChanged);
    connect(m_view, &PianoRollGraphicsView::keyRangeChanged, this,
            &IPianoRollCanvas::keyRangeChanged);
    connect(m_view, &PianoRollGraphicsView::keyHovered, this, &IPianoRollCanvas::keyHovered);
    connect(m_view, &PianoRollGraphicsView::keyHoverCleared, this,
            &IPianoRollCanvas::keyHoverCleared);
    connect(appStatus, &AppStatus::noteSelectionChanged, this,
            &LegacyPianoRollCanvasAdapter::syncSelection);
}

EditorCanvasBackend LegacyPianoRollCanvasAdapter::backend() const {
    return EditorCanvasBackend::Legacy;
}

QWidget *LegacyPianoRollCanvasAdapter::widget() const {
    return m_view;
}

QScrollBar *LegacyPianoRollCanvasAdapter::horizontalScrollBar() const {
    return m_view->horizontalScrollBar();
}

QScrollBar *LegacyPianoRollCanvasAdapter::verticalScrollBar() const {
    return m_view->verticalScrollBar();
}

void LegacyPianoRollCanvasAdapter::setDataContext(SingingClip *clip) {
    m_view->setDataContext(clip);
    syncSelection();
}

void LegacyPianoRollCanvasAdapter::setEditMode(const ClipEditorGlobal::PianoRollEditMode mode) {
    m_view->setEditMode(mode);
}

void LegacyPianoRollCanvasAdapter::setTrackColorIndex(const int index) {
    NoteView::setTrackColorIndex(index);
    m_view->viewport()->update();
}

EditorViewportState LegacyPianoRollCanvasAdapter::viewportState() const {
    EditorViewportState state;
    state.centerTick = (m_view->startTick() + m_view->endTick()) * 0.5;
    state.centerKey = m_view->centerKeyIndex();
    state.horizontalScale = m_view->scaleX();
    state.verticalScale = m_view->scaleY();
    state.startTick = m_view->startTick();
    state.endTick = m_view->endTick();
    state.topValue = m_view->topKeyIndex();
    state.bottomValue = m_view->bottomKeyIndex();
    state.viewportSize = m_view->viewport()->size();
    state.devicePixelRatio = m_view->devicePixelRatioF();
    return state;
}

void LegacyPianoRollCanvasAdapter::restoreViewportState(const EditorViewportState &state) {
    if (!setViewportScale(state.horizontalScale, state.verticalScale))
        return;
    centerAt(state.centerTick, state.centerKey);
    setPlaybackPosition(state.playbackPosition);
    setLastPlaybackPosition(state.lastPlaybackPosition);
}

bool LegacyPianoRollCanvasAdapter::centerAt(const double tick, const double keyIndex) {
    m_view->stopViewportAnimations();
    m_view->setViewportCenterAt(tick, keyIndex, false);
    return true;
}

bool LegacyPianoRollCanvasAdapter::setViewportScale(const double horizontalScale,
                                                    const double verticalScale) {
    return m_view->setViewportScale(horizontalScale, verticalScale);
}

double LegacyPianoRollCanvasAdapter::startTick() const {
    return m_view->startTick();
}

double LegacyPianoRollCanvasAdapter::endTick() const {
    return m_view->endTick();
}

double LegacyPianoRollCanvasAdapter::centerKeyIndex() const {
    return m_view->centerKeyIndex();
}

double LegacyPianoRollCanvasAdapter::scaleX() const {
    return m_view->scaleX();
}

double LegacyPianoRollCanvasAdapter::scaleY() const {
    return m_view->scaleY();
}

int LegacyPianoRollCanvasAdapter::horizontalBarValue() const {
    return m_view->horizontalBarValue();
}

void LegacyPianoRollCanvasAdapter::setPlaybackPosition(const double tick) {
    m_view->setPlaybackPosition(tick);
}

void LegacyPianoRollCanvasAdapter::setLastPlaybackPosition(const double tick) {
    m_view->setLastPlaybackPosition(tick);
}

void LegacyPianoRollCanvasAdapter::refreshSnapshot(const EditorDirtyDomains domains) {
    Q_UNUSED(domains);
    m_view->viewport()->update();
}

HistoryFocusVisibility
    LegacyPianoRollCanvasAdapter::focusVisibility(const HistoryFocus &focus) const {
    return m_view->focusVisibility(focus);
}

bool LegacyPianoRollCanvasAdapter::revealFocus(const HistoryFocus &focus, const bool animated) {
    const auto revealed = m_view->revealFocus(focus, animated);
    syncSelection();
    return revealed;
}

void LegacyPianoRollCanvasAdapter::syncSelection() const {
    const QSignalBlocker blocker(m_scene);
    const auto selected = appStatus->selectedNotes.get();
    for (auto *item : m_scene->items()) {
        if (auto *note = dynamic_cast<NoteView *>(item))
            note->setSelected(selected.contains(note->id()));
    }
}

void LegacyPianoRollCanvasAdapter::onWheelHorScale(QWheelEvent *event) {
    m_view->onWheelHorScale(event);
}

void LegacyPianoRollCanvasAdapter::onWheelVerScale(QWheelEvent *event) {
    m_view->onWheelVerScale(event);
}

void LegacyPianoRollCanvasAdapter::onWheelHorScroll(QWheelEvent *event) {
    m_view->onWheelHorScroll(event);
}

void LegacyPianoRollCanvasAdapter::onWheelVerScroll(QWheelEvent *event) {
    m_view->onWheelVerScroll(event);
}
