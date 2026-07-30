#include "RhiPianoRollCanvas.h"

#include "Controller/ClipController.h"
#include "Controller/ClipboardController.h"
#include "Global/ControllerGlobal.h"
#include "Global/AppGlobal.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/ClipboardDataModel/NotesParamsInfo.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Utils/AppColorPalette.h"
#include "UI/Dialogs/Note/PhonemeEditorDialog.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsViewHelper.h"
#include "UI/Views/EditorCanvas/RhiEditorCanvasWidget.h"

#include <lite/GUI/Controls/InlineTextEditOverlay.h>
#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/MusicBase/TimelineSnapUtils.h>

#include <QAction>
#include <QClipboard>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QHash>
#include <QJsonDocument>
#include <QLineF>
#include <QMap>
#include <QMenu>
#include <QMimeData>
#include <QScrollBar>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <climits>
#include <memory>

namespace {

    constexpr double pianoKeyHeight = 24.0;

} // namespace

RhiPianoRollCanvas::RhiPianoRollCanvas(QObject *parent)
    : IPianoRollCanvas(parent), m_widget(new RhiEditorCanvasWidget(EditorCanvasKind::PianoRoll)) {
    connect(m_widget, &RhiEditorCanvasWidget::scaleChanged, this, &IPianoRollCanvas::scaleChanged);
    connect(m_widget, &RhiEditorCanvasWidget::timeRangeChanged, this,
            [this](const double start, const double end) {
                emit timeRangeChanged(start + clipOffset(), end + clipOffset());
            });
    connect(m_widget, &RhiEditorCanvasWidget::secondaryRangeChanged, this,
            &IPianoRollCanvas::keyRangeChanged);
    connect(m_widget, &RhiEditorCanvasWidget::backendFailed, this,
            &IPianoRollCanvas::rendererFailed);
    connect(&m_scheduler, &RenderUpdateScheduler::updateRequested, this,
            &RhiPianoRollCanvas::publishSnapshot);
    connect(appStatus, &AppStatus::noteSelectionChanged, this,
            [this] { refreshSnapshot(EditorDirtyDomain::Selection); });
    connect(m_widget, &RhiEditorCanvasWidget::pointerPressed, this,
            &RhiPianoRollCanvas::onPointerPressed);
    connect(m_widget, &RhiEditorCanvasWidget::pointerMoved, this,
            &RhiPianoRollCanvas::onPointerMoved);
    connect(m_widget, &RhiEditorCanvasWidget::pointerLeft, this, [this] {
        m_pointerInside = false;
        m_hoveredNoteId = -1;
        m_hoveredKey = -1;
        m_anchorMergeCurveIndex = -1;
        m_anchorMergeNodeIndex = -1;
        emit keyHoverCleared();
        refreshSnapshot(EditorDirtyDomain::Selection | EditorDirtyDomain::Overlay);
    });
    connect(m_widget, &RhiEditorCanvasWidget::pointerReleased, this,
            &RhiPianoRollCanvas::onPointerReleased);
    connect(m_widget, &RhiEditorCanvasWidget::pointerDoubleClicked, this,
            [this](const QPointF &position, const EditorHitResult &hit,
                   const Qt::MouseButton button,
                   const Qt::KeyboardModifiers) { onPointerDoubleClicked(position, hit, button); });
    connect(m_widget, &RhiEditorCanvasWidget::contextMenuRequested, this,
            &RhiPianoRollCanvas::showContextMenu);
    connect(m_widget, &RhiEditorCanvasWidget::keyPressed, this, &RhiPianoRollCanvas::onKeyPressed);
    connect(m_widget, &RhiEditorCanvasWidget::interactionCanceled, this,
            &RhiPianoRollCanvas::cancelInteraction);
    connect(m_widget, &RhiEditorCanvasWidget::timeRangeChanged, this,
            &RhiPianoRollCanvas::ensureVisibleSnapshot);
    connect(m_widget, &RhiEditorCanvasWidget::secondaryRangeChanged, this,
            &RhiPianoRollCanvas::ensureVisibleSnapshot);
    connect(m_widget, &RhiEditorCanvasWidget::canvasSizeChanged, this,
            [this] { refreshSnapshot(EditorDirtyDomain::Geometry); });
    m_inlineEditor = new InlineTextEditOverlay(m_widget);
    connect(m_inlineEditor, &InlineTextEditOverlay::textSubmitted, this,
            &RhiPianoRollCanvas::finishInlineEditing);
    connect(m_inlineEditor, &InlineTextEditOverlay::editCancelled, this, [this] {
        m_inlineEditingNoteId = -1;
        m_inlineEditingPronunciation = false;
    });
    connect(m_inlineEditor, &InlineTextEditOverlay::navigationRequested, this,
            [this](const QString &text, const bool backwards) {
                const auto currentId = m_inlineEditingNoteId;
                const auto pronunciation = m_inlineEditingPronunciation;
                finishInlineEditing(text);
                if (!m_clip)
                    return;
                const auto notes = m_clip->notes().toList();
                auto index = -1;
                for (qsizetype i = 0; i < notes.size(); ++i) {
                    if (notes.at(i)->id() == currentId) {
                        index = static_cast<int>(i);
                        break;
                    }
                }
                const auto nextIndex = backwards ? index - 1 : index + 1;
                if (nextIndex >= 0 && nextIndex < notes.size()) {
                    if (pronunciation)
                        startPronunciationEditing(notes.at(nextIndex)->id());
                    else
                        startLyricEditing(notes.at(nextIndex)->id());
                }
            });
    m_widget->setSceneLength(AppGlobal::ticksPerWholeNote * 80);
    refreshSnapshot(EditorDirtyDomain::All);
}

RhiPianoRollCanvas::~RhiPianoRollCanvas() {
    if (m_inlineEditor && m_inlineEditor->isEditing())
        m_inlineEditor->dismiss(false);
    finishEditTransaction(false);
}

EditorCanvasBackend RhiPianoRollCanvas::backend() const {
    return EditorCanvasBackend::ExperimentalRhi;
}

QWidget *RhiPianoRollCanvas::widget() const {
    return m_widget;
}

QScrollBar *RhiPianoRollCanvas::horizontalScrollBar() const {
    return m_widget->horizontalScrollBar();
}

QScrollBar *RhiPianoRollCanvas::verticalScrollBar() const {
    return m_widget->verticalScrollBar();
}

void RhiPianoRollCanvas::setDataContext(SingingClip *clip) {
    if (m_clip == clip)
        return;
    cancelInteraction();
    if (m_inlineEditor && m_inlineEditor->isEditing())
        m_inlineEditor->dismiss(true);
    m_inlineEditingNoteId = -1;
    m_inlineEditingPronunciation = false;
    m_selectedAnchors.clear();
    m_anchorSelectionBeforeRect.clear();
    m_anchorMergeCurveIndex = -1;
    m_anchorMergeNodeIndex = -1;
    if (m_clip)
        disconnect(m_clip, nullptr, this, nullptr);
    m_clip = clip;
    if (m_clip) {
        connect(m_clip, &SingingClip::noteChanged, this,
                [this] { refreshSnapshot(EditorDirtyDomain::Geometry | EditorDirtyDomain::Text); });
        connect(m_clip, &Clip::propertyChanged, this,
                [this] { refreshSnapshot(EditorDirtyDomain::Geometry); });
        connect(m_clip, &SingingClip::paramChanged, this, [this] {
            refreshSnapshot(EditorDirtyDomain::Geometry | EditorDirtyDomain::Style);
        });
    }
    m_widget->setSceneLength(m_clip ? m_clip->length() : AppGlobal::ticksPerWholeNote * 80);
    positionViewportAtClipContent();
    QTimer::singleShot(0, this, [this, clip] {
        if (m_clip == clip)
            positionViewportAtClipContent();
    });
    refreshSnapshot(EditorDirtyDomain::All);
    emit timeRangeChanged(startTick(), endTick());
}

void RhiPianoRollCanvas::setEditMode(const ClipEditorGlobal::PianoRollEditMode mode) {
    if (m_editMode == mode)
        return;
    cancelInteraction();
    if (mode != ClipEditorGlobal::EditPitchAnchor) {
        m_selectedAnchors.clear();
        m_anchorSelectionBeforeRect.clear();
        m_anchorMergeCurveIndex = -1;
        m_anchorMergeNodeIndex = -1;
    }
    m_editMode = mode;
    refreshSnapshot(EditorDirtyDomain::Overlay);
}

void RhiPianoRollCanvas::setTrackColorIndex(const int index) {
    if (m_trackColorIndex == index)
        return;
    m_trackColorIndex = index;
    refreshSnapshot(EditorDirtyDomain::Style);
}

EditorViewportState RhiPianoRollCanvas::viewportState() const {
    auto state = m_widget->viewportState();
    const auto offset = clipOffset();
    state.centerTick += offset;
    state.startTick += offset;
    state.endTick += offset;
    state.playbackPosition += offset;
    state.lastPlaybackPosition += offset;
    return state;
}

void RhiPianoRollCanvas::restoreViewportState(const EditorViewportState &state) {
    auto localState = state;
    const auto offset = clipOffset();
    localState.centerTick -= offset;
    localState.startTick -= offset;
    localState.endTick -= offset;
    localState.playbackPosition -= offset;
    localState.lastPlaybackPosition -= offset;
    m_widget->restoreViewportState(localState);
}

bool RhiPianoRollCanvas::centerAt(const double tick, const double keyIndex) {
    if (!std::isfinite(tick) || !std::isfinite(keyIndex))
        return false;
    m_widget->setViewportCenter(tick - clipOffset(), keyIndex);
    return true;
}

bool RhiPianoRollCanvas::setViewportScale(const double horizontalScale,
                                          const double verticalScale) {
    return m_widget->setViewportScale(horizontalScale, verticalScale);
}

double RhiPianoRollCanvas::startTick() const {
    return m_widget->startTick() + clipOffset();
}

double RhiPianoRollCanvas::endTick() const {
    return m_widget->endTick() + clipOffset();
}

double RhiPianoRollCanvas::centerKeyIndex() const {
    return m_widget->centerSecondaryValue();
}

double RhiPianoRollCanvas::scaleX() const {
    return m_widget->scaleX();
}

double RhiPianoRollCanvas::scaleY() const {
    return m_widget->scaleY();
}

int RhiPianoRollCanvas::horizontalBarValue() const {
    return m_widget->horizontalScrollBar()->value();
}

void RhiPianoRollCanvas::setPlaybackPosition(const double tick) {
    m_widget->setPlaybackPosition(tick - clipOffset());
}

void RhiPianoRollCanvas::setLastPlaybackPosition(const double tick) {
    m_widget->setLastPlaybackPosition(tick - clipOffset());
}

void RhiPianoRollCanvas::refreshSnapshot(const EditorDirtyDomains domains) {
    m_scheduler.request(domains);
}

HistoryFocusVisibility RhiPianoRollCanvas::focusVisibility(const HistoryFocus &focus) const {
    if (focus.kind != HistoryFocusKind::PianoRollNotes || !focus.isValid() || !m_clip ||
        (focus.containerId >= 0 && focus.containerId != m_clip->id()))
        return HistoryFocusVisibility::Unavailable;
    const auto state = viewportState();
    const auto start = focus.ticksAreLocal ? focus.tickStart + clipOffset() : focus.tickStart;
    const auto end = focus.ticksAreLocal ? focus.tickEnd + clipOffset() : focus.tickEnd;
    const auto tickVisible = end >= state.startTick && start <= state.endTick;
    const auto keyVisible =
        focus.valueStart <= state.topValue && focus.valueEnd >= state.bottomValue;
    return tickVisible && keyVisible ? HistoryFocusVisibility::Visible
                                     : HistoryFocusVisibility::ScrollRequired;
}

bool RhiPianoRollCanvas::revealFocus(const HistoryFocus &focus, const bool animated) {
    Q_UNUSED(animated);
    if (focus.kind != HistoryFocusKind::PianoRollNotes || !focus.isValid())
        return false;
    appStatus->selectedNotes = focus.objectIds;
    const auto start = focus.ticksAreLocal ? focus.tickStart + clipOffset() : focus.tickStart;
    const auto end = focus.ticksAreLocal ? focus.tickEnd + clipOffset() : focus.tickEnd;
    return centerAt((start + end) * 0.5, (focus.valueStart + focus.valueEnd) * 0.5);
}

void RhiPianoRollCanvas::onWheelHorScale(QWheelEvent *event) {
    m_widget->onWheelHorScale(event);
}

void RhiPianoRollCanvas::onWheelVerScale(QWheelEvent *event) {
    m_widget->onWheelVerScale(event);
}

void RhiPianoRollCanvas::onWheelHorScroll(QWheelEvent *event) {
    m_widget->onWheelHorScroll(event);
}

void RhiPianoRollCanvas::onWheelVerScroll(QWheelEvent *event) {
    m_widget->onWheelVerScroll(event);
}

void RhiPianoRollCanvas::onPointerPressed(const QPointF &position, const EditorHitResult &hit,
                                          const Qt::MouseButton button,
                                          const Qt::KeyboardModifiers modifiers) {
    if (button != Qt::LeftButton || !m_clip)
        return;
    m_widget->setFocus(Qt::MouseFocusReason);
    m_pressPosition = position;
    m_lastPosition = position;
    m_pressedNoteId = hit.objectId;
    m_previewDeltaTick = 0;
    m_previewDeltaKey = 0;
    m_anchorPreviewDeltaTick = 0;
    m_anchorPreviewDeltaValue = 0;
    m_selectionRect = {};
    m_eraseNoteIds.clear();

    if (m_editMode == ClipEditorGlobal::DrawPitch || m_editMode == ClipEditorGlobal::ErasePitch ||
        m_editMode == ClipEditorGlobal::FreezePitch) {
        m_interaction = m_editMode == ClipEditorGlobal::DrawPitch ? Interaction::DrawPitch
                                                                  : Interaction::ErasePitch;
        m_pitchStroke = {position};
        beginPitchTransaction();
        refreshSnapshot(EditorDirtyDomain::Overlay);
        return;
    }

    if (m_editMode == ClipEditorGlobal::EditPitchAnchor) {
        const auto [curveIndex, nodeIndex] = anchorAt(position);
        if (curveIndex >= 0 && nodeIndex >= 0) {
            if (curveIndex == m_anchorMergeCurveIndex && nodeIndex == m_anchorMergeNodeIndex &&
                !m_selectedAnchors.isEmpty()) {
                const auto sourceCurveIndex = m_selectedAnchors.constFirst().curveIndex;
                beginPitchTransaction();
                mergeAnchorCurves(sourceCurveIndex, curveIndex);
                finishEditTransaction(true);
                m_selectedAnchors.clear();
                m_anchorMergeCurveIndex = -1;
                m_anchorMergeNodeIndex = -1;
                refreshSnapshot(EditorDirtyDomain::All);
                return;
            }
            if (!isAnchorSelected(curveIndex, nodeIndex))
                selectAnchor(curveIndex, nodeIndex);
            else
                selectAnchorsInRect({}, true);
            m_interaction = Interaction::MoveAnchor;
            beginPitchTransaction();
            refreshSnapshot(EditorDirtyDomain::Overlay);
        } else if (!m_selectedAnchors.isEmpty()) {
            insertAnchorAt(position);
        } else {
            m_interaction = Interaction::RectSelect;
            m_selectionRect = QRectF(position, position);
            m_anchorSelectionBeforeRect = modifiers.testFlag(Qt::ControlModifier)
                                              ? m_selectedAnchors
                                              : QVector<AnchorSelection>{};
        }
        return;
    }

    if (m_editMode == ClipEditorGlobal::SplitNote) {
        if (hit.isValid())
            PianoRollGraphicsViewHelper::splitNote(hit.objectId,
                                                   localTickAt(position.x()) + clipOffset());
        return;
    }

    if (m_editMode == ClipEditorGlobal::EraseNote) {
        m_interaction = Interaction::Erase;
        if (hit.isValid())
            m_eraseNoteIds.insert(hit.objectId);
        beginEditTransaction(m_eraseNoteIds.values());
        refreshSnapshot(EditorDirtyDomain::Overlay);
        return;
    }

    auto *note = findNote(hit.objectId);
    if (m_editMode == ClipEditorGlobal::DrawNote && !note) {
        m_interaction = Interaction::Draw;
        m_drawStart = snappedLocalTick(localTickAt(position.x()));
        m_drawLength = TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
        m_drawKey = keyAt(position.y());
        beginEditTransaction({});
        refreshSnapshot(EditorDirtyDomain::Overlay);
        return;
    }

    if (!note) {
        if (!modifiers.testFlag(Qt::ControlModifier))
            appStatus->selectedNotes = QList<int>{};
        m_interaction = Interaction::RectSelect;
        m_selectionRect = QRectF(position, position);
        refreshSnapshot(EditorDirtyDomain::Selection | EditorDirtyDomain::Overlay);
        return;
    }

    auto selected = appStatus->selectedNotes.get();
    if (modifiers.testFlag(Qt::ControlModifier)) {
        if (selected.contains(note->id()))
            selected.removeAll(note->id());
        else
            selected.append(note->id());
        appStatus->selectedNotes = selected;
        if (!selected.contains(note->id()))
            return;
    } else if (!selected.contains(note->id())) {
        appStatus->selectedNotes = QList<int>{note->id()};
        selected = {note->id()};
    }

    m_originalStart = note->localStart();
    m_originalLength = note->length();
    m_originalKey = note->keyIndex();
    if (hit.part == EditorHitResult::Part::LeftEdge)
        m_interaction = Interaction::ResizeLeft;
    else if (hit.part == EditorHitResult::Part::RightEdge)
        m_interaction = Interaction::ResizeRight;
    else
        m_interaction = Interaction::Move;
    beginEditTransaction(selected);
}

void RhiPianoRollCanvas::onPointerMoved(const QPointF &position, const EditorHitResult &hit,
                                        const Qt::MouseButtons buttons,
                                        const Qt::KeyboardModifiers modifiers) {
    m_pointerInside = true;
    m_lastPosition = position;
    const auto hoveredKey = keyAt(position.y());
    if (hoveredKey != m_hoveredKey) {
        m_hoveredKey = hoveredKey;
        emit keyHovered(hoveredKey);
    }
    if (m_hoveredNoteId != hit.objectId) {
        m_hoveredNoteId = hit.objectId;
        refreshSnapshot(EditorDirtyDomain::Selection);
    }
    if (m_editMode == ClipEditorGlobal::EditPitchAnchor && !buttons.testFlag(Qt::LeftButton)) {
        updateAnchorMergeCandidate(position);
        refreshSnapshot(EditorDirtyDomain::Overlay);
    }
    if (!buttons.testFlag(Qt::LeftButton) || m_interaction == Interaction::None)
        return;

    autoScrollFor(position);
    if (m_interaction == Interaction::DrawPitch || m_interaction == Interaction::ErasePitch) {
        if (m_pitchStroke.isEmpty() ||
            QLineF(m_pitchStroke.constLast(), position).length() >= 1.0) {
            m_pitchStroke.append(position);
        }
        refreshSnapshot(EditorDirtyDomain::Overlay);
        return;
    }
    if (m_interaction == Interaction::MoveAnchor) {
        auto deltaTick = localTickAt(position.x()) - localTickAt(m_pressPosition.x());
        auto deltaValue = qRound((m_pressPosition.y() - position.y()) * 100.0 / pianoKeyHeight);
        auto minimumTick = INT_MAX;
        auto minimumValue = INT_MAX;
        auto maximumValue = INT_MIN;
        for (const auto &selection : m_selectedAnchors) {
            minimumTick = qMin(minimumTick, selection.startTick);
            minimumValue = qMin(minimumValue, selection.startValue);
            maximumValue = qMax(maximumValue, selection.startValue);
        }
        if (minimumTick != INT_MAX) {
            deltaTick = qMax(-minimumTick, deltaTick);
            deltaValue = qBound(-minimumValue, deltaValue, 12700 - maximumValue);
        }
        m_anchorPreviewDeltaTick = deltaTick;
        m_anchorPreviewDeltaValue = deltaValue;
        refreshSnapshot(EditorDirtyDomain::Overlay);
        return;
    }
    const auto rawDelta = localTickAt(position.x()) - localTickAt(m_pressPosition.x());
    const auto snappedDelta =
        modifiers.testFlag(Qt::AltModifier)
            ? rawDelta
            : snappedLocalTick(m_originalStart + rawDelta, true) - m_originalStart;
    if (m_interaction == Interaction::Draw) {
        const auto right = snappedLocalTick(localTickAt(position.x()));
        const auto minimum = TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
        m_drawLength = qMax(minimum, right - m_drawStart);
    } else if (m_interaction == Interaction::Move) {
        m_previewDeltaTick = snappedDelta;
        const auto targetKey = keyAt(position.y());
        m_previewDeltaKey = targetKey - m_originalKey;
        int minimumKey = 127;
        int maximumKey = 0;
        for (const auto id : appStatus->selectedNotes.get()) {
            if (const auto *selectedNote = findNote(id)) {
                minimumKey = qMin(minimumKey, selectedNote->keyIndex());
                maximumKey = qMax(maximumKey, selectedNote->keyIndex());
            }
        }
        m_previewDeltaKey = qBound(-minimumKey, m_previewDeltaKey, 127 - maximumKey);
    } else if (m_interaction == Interaction::ResizeLeft) {
        m_previewDeltaTick = qBound(-m_originalStart, snappedDelta, m_originalLength - 1);
    } else if (m_interaction == Interaction::ResizeRight) {
        m_previewDeltaTick = qMax(1 - m_originalLength, snappedDelta);
    } else if (m_interaction == Interaction::Erase) {
        if (hit.isValid())
            m_eraseNoteIds.insert(hit.objectId);
    } else if (m_interaction == Interaction::RectSelect) {
        m_selectionRect = QRectF(m_pressPosition, position).normalized();
        if (m_editMode == ClipEditorGlobal::IntervalSelect) {
            m_selectionRect.setTop(0.0);
            m_selectionRect.setBottom(128.0 * pianoKeyHeight);
        }
        if (m_editMode == ClipEditorGlobal::EditPitchAnchor)
            selectAnchorsInRect(m_selectionRect, modifiers.testFlag(Qt::ControlModifier));
    }
    refreshSnapshot(EditorDirtyDomain::Overlay | EditorDirtyDomain::Selection);
}

void RhiPianoRollCanvas::onPointerReleased(const QPointF &position, const EditorHitResult &hit,
                                           const Qt::MouseButton button,
                                           const Qt::KeyboardModifiers modifiers) {
    Q_UNUSED(position);
    Q_UNUSED(hit);
    if (button != Qt::LeftButton || m_interaction == Interaction::None)
        return;

    auto changed = false;
    if (m_interaction == Interaction::DrawPitch || m_interaction == Interaction::ErasePitch) {
        applyPitchStroke(m_interaction == Interaction::ErasePitch);
        changed = m_pitchStroke.size() >= 2;
    } else if (m_interaction == Interaction::MoveAnchor) {
        changed = m_anchorPreviewDeltaTick != 0 || m_anchorPreviewDeltaValue != 0;
        if (changed)
            commitAnchorMove();
    } else if (m_interaction == Interaction::Draw) {
        PianoRollGraphicsViewHelper::drawNote(m_drawStart, m_drawLength, m_drawKey);
        changed = true;
    } else if (m_interaction == Interaction::Move &&
               (m_previewDeltaTick != 0 || m_previewDeltaKey != 0)) {
        clipController->onMoveNotes(appStatus->selectedNotes, m_previewDeltaTick,
                                    m_previewDeltaKey);
        changed = true;
    } else if (m_interaction == Interaction::ResizeLeft && m_previewDeltaTick != 0) {
        clipController->onResizeNotesLeft({m_pressedNoteId}, m_previewDeltaTick);
        changed = true;
    } else if (m_interaction == Interaction::ResizeRight && m_previewDeltaTick != 0) {
        clipController->onResizeNotesRight({m_pressedNoteId}, m_previewDeltaTick);
        changed = true;
    } else if (m_interaction == Interaction::Erase && !m_eraseNoteIds.isEmpty()) {
        clipController->onRemoveNotes(m_eraseNoteIds.values());
        changed = true;
    } else if (m_interaction == Interaction::RectSelect && !m_selectionRect.isNull()) {
        if (m_editMode == ClipEditorGlobal::EditPitchAnchor) {
            selectAnchorsInRect(m_selectionRect, modifiers.testFlag(Qt::ControlModifier));
        } else {
            auto selected = modifiers.testFlag(Qt::ControlModifier) ? appStatus->selectedNotes.get()
                                                                    : QList<int>{};
            for (const auto *note : m_clip->notes()) {
                const auto x = note->localStart() * ClipEditorGlobal::pixelsPerQuarterNote /
                               static_cast<double>(AppGlobal::ticksPerQuarterNote);
                const auto width = note->length() * ClipEditorGlobal::pixelsPerQuarterNote /
                                   static_cast<double>(AppGlobal::ticksPerQuarterNote);
                const auto y = (127 - note->keyIndex()) * pianoKeyHeight;
                if (m_selectionRect.intersects({x, y, width, pianoKeyHeight}) &&
                    !selected.contains(note->id()))
                    selected.append(note->id());
            }
            appStatus->selectedNotes = selected;
        }
    }
    finishEditTransaction(changed);
    m_interaction = Interaction::None;
    m_selectionRect = {};
    m_eraseNoteIds.clear();
    m_pitchStroke.clear();
    m_previewDeltaTick = 0;
    m_previewDeltaKey = 0;
    m_anchorPreviewDeltaTick = 0;
    m_anchorPreviewDeltaValue = 0;
    m_anchorSelectionBeforeRect.clear();
    refreshSnapshot(EditorDirtyDomain::All);
}

void RhiPianoRollCanvas::onPointerDoubleClicked(const QPointF &position, const EditorHitResult &hit,
                                                const Qt::MouseButton button) {
    if (button != Qt::LeftButton || !m_clip)
        return;
    if (m_editMode == ClipEditorGlobal::EditPitchAnchor) {
        insertAnchorAt(position);
        return;
    }
    if (m_editMode != ClipEditorGlobal::Select)
        return;
    if (hit.isValid()) {
        startLyricEditing(hit.objectId);
        return;
    }
    const auto start = snappedLocalTick(localTickAt(position.x()));
    const auto length = TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
    beginEditTransaction({});
    PianoRollGraphicsViewHelper::drawNote(start, length, keyAt(position.y()));
    finishEditTransaction(true);
}

void RhiPianoRollCanvas::showContextMenu(const QPointF &position, const EditorHitResult &hit,
                                         const QPoint &globalPosition) {
    if (!m_clip)
        return;
    if (m_editMode == ClipEditorGlobal::EditPitchAnchor) {
        const auto [curveIndex, nodeIndex] = anchorAt(position);
        if (curveIndex < 0 || nodeIndex < 0)
            return;
        if (!isAnchorSelected(curveIndex, nodeIndex))
            selectAnchor(curveIndex, nodeIndex);

        const auto *pitch = m_clip->params.getParamByName(ParamInfo::Pitch);
        const auto &curves = pitch->curves(Param::Edited);
        const auto *curve = static_cast<const AnchorCurve *>(curves.at(curveIndex));
        const auto nodes = curve->nodes().toList();
        const auto currentMode = nodes.at(nodeIndex)->interpMode();
        auto allSame = true;
        auto allAreLast = true;
        for (const auto &selection : m_selectedAnchors) {
            const auto *selectedCurve =
                static_cast<const AnchorCurve *>(curves.at(selection.curveIndex));
            const auto selectedNodes = selectedCurve->nodes().toList();
            allSame &= selectedNodes.at(selection.nodeIndex)->interpMode() == currentMode;
            allAreLast &= selection.nodeIndex == selectedNodes.size() - 1;
        }

        QMenu menu(m_widget);
        auto *linearAction = menu.addAction(tr("Linear"));
        linearAction->setCheckable(true);
        linearAction->setChecked(allSame && currentMode == AnchorNode::Linear);
        linearAction->setEnabled(!allAreLast);
        connect(linearAction, &QAction::triggered, this,
                [this] { setSelectedAnchorInterpolation(AnchorNode::Linear); });
        auto *hermiteAction = menu.addAction(tr("Hermite"));
        hermiteAction->setCheckable(true);
        hermiteAction->setChecked(allSame && currentMode == AnchorNode::Hermite);
        hermiteAction->setEnabled(!allAreLast);
        connect(hermiteAction, &QAction::triggered, this,
                [this] { setSelectedAnchorInterpolation(AnchorNode::Hermite); });
        menu.addSeparator();
        const auto deleteAction = menu.addAction(tr("&Delete"));
        connect(deleteAction, &QAction::triggered, this, [this] {
            beginPitchTransaction();
            commitAnchorMove(true);
            finishEditTransaction(true);
            m_selectedAnchors.clear();
            refreshSnapshot(EditorDirtyDomain::All);
        });
        menu.exec(globalPosition);
        return;
    }
    if (hit.isValid() && !appStatus->selectedNotes.get().contains(hit.objectId))
        appStatus->selectedNotes = QList<int>{hit.objectId};
    QMenu menu(m_widget);
    if (hit.isValid()) {
        auto *note = findNote(hit.objectId);
        if (!note)
            return;
        auto *languageMenu = menu.addMenu(tr("Language"));
        for (const auto &language : AppGlobal::languageNames) {
            auto *action = languageMenu->addAction(language);
            action->setCheckable(true);
            action->setChecked(note->language() == language);
            connect(action, &QAction::triggered, this, [language] {
                clipController->onNoteLanguagesEdited(appStatus->selectedNotes, language);
            });
        }
        const auto lyricAction = menu.addAction(tr("Edit lyric..."));
        connect(lyricAction, &QAction::triggered, this,
                [this, id = hit.objectId] { startLyricEditing(id); });
        const auto pronunciationAction = menu.addAction(tr("Edit pronunciation..."));
        connect(pronunciationAction, &QAction::triggered, this,
                [this, id = hit.objectId] { startPronunciationEditing(id); });
        const auto fillAction = menu.addAction(tr("Fill lyrics..."));
        connect(fillAction, &QAction::triggered, this,
                [this] { clipController->onFillLyric(m_widget); });
        const auto searchAction = menu.addAction(tr("Search lyrics..."));
        connect(searchAction, &QAction::triggered, this,
                [this] { clipController->onSearchLyric(m_widget); });
        const auto phonemeAction = menu.addAction(tr("Edit Phonemes..."));
        phonemeAction->setEnabled(appStatus->selectedNotes.get().size() == 1);
        connect(phonemeAction, &QAction::triggered, this,
                [this, id = hit.objectId] { openPhonemeEditor(id); });
        const auto splitAction = menu.addAction(tr("Split Note"));
        connect(splitAction, &QAction::triggered, this, [this, id = hit.objectId, position] {
            PianoRollGraphicsViewHelper::splitNote(id, localTickAt(position.x()) + clipOffset());
        });
        menu.addSeparator();
        const auto copyAction = menu.addAction(tr("&Copy"));
        connect(copyAction, &QAction::triggered, clipboardController, &ClipboardController::copy);
        const auto cutAction = menu.addAction(tr("Cu&t"));
        connect(cutAction, &QAction::triggered, clipboardController, &ClipboardController::cut);
        const auto deleteAction = menu.addAction(tr("&Delete"));
        connect(deleteAction, &QAction::triggered, clipController,
                &ClipController::onDeleteSelectedNotes);
    } else {
        const auto pasteAction = menu.addAction(tr("&Paste"));
        const auto *mimeData = QGuiApplication::clipboard()->mimeData();
        const auto hasNoteData = mimeData && mimeData->hasFormat(ControllerGlobal::ElemMimeType.at(
                                                 ControllerGlobal::NoteWithParams));
        pasteAction->setEnabled(hasNoteData);
        if (hasNoteData) {
            const auto info = NotesParamsInfo::deserializeFromJson(
                QJsonDocument::fromJson(mimeData->data(ControllerGlobal::ElemMimeType.at(
                                            ControllerGlobal::NoteWithParams)))
                    .object());
            const auto pasteTick = localTickAt(position.x()) + clipOffset();
            const auto previewTick = snappedLocalTick(localTickAt(position.x()), true);
            auto firstStart = INT_MAX;
            for (const auto *note : info.selectedNotes)
                firstStart = qMin(firstStart, note->localStart());
            connect(pasteAction, &QAction::triggered, this,
                    [info, pasteTick] { clipController->pasteNotesWithParams(info, pasteTick); });
            connect(pasteAction, &QAction::hovered, this, [this, info, previewTick, firstStart] {
                if (!m_pastePreviewRects.isEmpty())
                    return;
                for (const auto *note : info.selectedNotes) {
                    const auto start = previewTick + note->localStart() - firstStart;
                    const auto x = start * ClipEditorGlobal::pixelsPerQuarterNote /
                                   static_cast<double>(AppGlobal::ticksPerQuarterNote);
                    const auto width = note->length() * ClipEditorGlobal::pixelsPerQuarterNote /
                                       static_cast<double>(AppGlobal::ticksPerQuarterNote);
                    auto fill = AppColorPalette::instance()->noteBackground(m_trackColorIndex);
                    fill.setAlpha(90);
                    const QRectF bounds(x, (127 - note->keyIndex()) * pianoKeyHeight + 1.5,
                                        qMax(1.0, width), pianoKeyHeight - 3.0);
                    m_pastePreviewRects.append({
                        .objectId = -1,
                        .bounds = bounds,
                        .fill = fill,
                        .border = QColor(225, 235, 255, 150),
                        .layer = 30,
                    });
                    QFont font;
                    font.setPixelSize(12);
                    m_pastePreviewTexts.append({
                        .text = note->lyric(),
                        .baseline = {bounds.left() + 4.0, bounds.top() + 15.0},
                        .color = QColor(230, 235, 245, 160),
                        .font = font,
                        .clip = bounds.adjusted(2.0, 1.0, -2.0, -1.0),
                        .layer = 30,
                    });
                }
                refreshSnapshot(EditorDirtyDomain::Overlay);
            });
            connect(&menu, &QMenu::aboutToHide, this, &RhiPianoRollCanvas::clearPastePreview);
        }
    }
    menu.exec(globalPosition);
}

void RhiPianoRollCanvas::onKeyPressed(const int key, const Qt::KeyboardModifiers modifiers) {
    if (key == Qt::Key_Escape && m_editMode == ClipEditorGlobal::EditPitchAnchor) {
        m_selectedAnchors.clear();
        m_anchorMergeCurveIndex = -1;
        m_anchorMergeNodeIndex = -1;
        refreshSnapshot(EditorDirtyDomain::Overlay);
    } else if ((key == Qt::Key_Delete || key == Qt::Key_Backspace) &&
               m_editMode == ClipEditorGlobal::EditPitchAnchor && !m_selectedAnchors.isEmpty()) {
        beginPitchTransaction();
        commitAnchorMove(true);
        finishEditTransaction(true);
        m_selectedAnchors.clear();
    } else if (key == Qt::Key_Delete || key == Qt::Key_Backspace) {
        clipController->onDeleteSelectedNotes();
    } else if (modifiers.testFlag(Qt::ControlModifier) && key == Qt::Key_A) {
        clipController->onSelectAllNotes();
    } else if (modifiers.testFlag(Qt::ControlModifier) && key == Qt::Key_C) {
        clipController->copySelectedNotesWithParams();
    } else if (modifiers.testFlag(Qt::ControlModifier) && key == Qt::Key_X) {
        clipController->cutSelectedNotesWithParams();
    } else if (modifiers.testFlag(Qt::ControlModifier) && key == Qt::Key_V) {
        clipboardController->paste();
    }
}

void RhiPianoRollCanvas::cancelInteraction() {
    if (m_interaction == Interaction::None)
        return;
    finishEditTransaction(false);
    m_interaction = Interaction::None;
    m_selectionRect = {};
    m_eraseNoteIds.clear();
    m_pitchStroke.clear();
    m_previewDeltaTick = 0;
    m_previewDeltaKey = 0;
    m_anchorPreviewDeltaTick = 0;
    m_anchorPreviewDeltaValue = 0;
    m_anchorMergeCurveIndex = -1;
    m_anchorMergeNodeIndex = -1;
    refreshSnapshot(EditorDirtyDomain::Overlay);
}

void RhiPianoRollCanvas::autoScrollFor(const QPointF &logicalPosition) {
    const auto state = m_widget->viewportState();
    const auto cameraX =
        state.startTick * ClipEditorGlobal::pixelsPerQuarterNote / AppGlobal::ticksPerQuarterNote;
    const auto cameraY = (127.0 - state.topValue) * pianoKeyHeight;
    const auto viewportX = (logicalPosition.x() - cameraX) * m_widget->scaleX();
    const auto viewportY = (logicalPosition.y() - cameraY) * m_widget->scaleY();
    constexpr int margin = 28;
    constexpr int step = 18;
    if (viewportX < margin)
        m_widget->horizontalScrollBar()->setValue(m_widget->horizontalScrollBar()->value() - step);
    else if (viewportX > m_widget->width() - margin)
        m_widget->horizontalScrollBar()->setValue(m_widget->horizontalScrollBar()->value() + step);
    if (m_editMode != ClipEditorGlobal::IntervalSelect) {
        if (viewportY < margin)
            m_widget->verticalScrollBar()->setValue(m_widget->verticalScrollBar()->value() - step);
        else if (viewportY > m_widget->height() - margin)
            m_widget->verticalScrollBar()->setValue(m_widget->verticalScrollBar()->value() + step);
    }
}

void RhiPianoRollCanvas::beginPitchTransaction() {
    if (!m_clip || editSessionManager->hasActiveTransaction())
        return;
    editSessionManager->beginTransaction(AppStatus::EditObjectType::Param, m_clip->id(), {}, {}, {},
                                         {ParamInfo::Pitch});
    appStatus->currentEditObject = AppStatus::EditObjectType::Param;
    m_editTransactionActive = true;
}

void RhiPianoRollCanvas::applyPitchStroke(const bool erase) {
    if (!m_clip || m_pitchStroke.size() < 2)
        return;

    const auto *pitch = m_clip->params.getParamByName(ParamInfo::Pitch);
    QMap<int, int> samples;
    QList<Curve *> result;
    for (const auto *curve : pitch->curves(Param::Edited)) {
        if (curve->type() == Curve::Anchor) {
            result.append(new AnchorCurve(*static_cast<const AnchorCurve *>(curve)));
            continue;
        }
        if (curve->type() != Curve::Draw)
            continue;
        const auto *draw = static_cast<const DrawCurve *>(curve);
        for (qsizetype i = 0; i < draw->values().size(); ++i)
            samples.insert(draw->localStart() + static_cast<int>(i) * draw->step,
                           draw->values().at(i));
    }

    QVector<QPointF> stroke = m_pitchStroke;
    std::ranges::sort(stroke, [this](const QPointF &left, const QPointF &right) {
        return localTickAt(left.x()) < localTickAt(right.x());
    });
    const auto firstTick = localTickAt(stroke.constFirst().x()) / 5 * 5;
    const auto lastTick = qMax(firstTick + 5, localTickAt(stroke.constLast().x()) / 5 * 5);
    if (erase) {
        auto it = samples.lowerBound(firstTick);
        while (it != samples.end() && it.key() <= lastTick)
            it = samples.erase(it);
    } else {
        qsizetype segment = 0;
        for (auto tick = firstTick; tick <= lastTick; tick += 5) {
            while (segment + 1 < stroke.size() && localTickAt(stroke.at(segment + 1).x()) < tick) {
                ++segment;
            }
            const auto &left = stroke.at(segment);
            const auto &right =
                stroke.at(qMin(segment + 1, static_cast<qsizetype>(stroke.size() - 1)));
            const auto leftTick = localTickAt(left.x());
            const auto rightTick = localTickAt(right.x());
            const auto ratio =
                rightTick == leftTick
                    ? 0.0
                    : qBound(0.0, (tick - leftTick) / static_cast<double>(rightTick - leftTick),
                             1.0);
            const auto y = left.y() + (right.y() - left.y()) * ratio;
            samples.insert(tick, qBound(0, qRound((127.5 - y / pianoKeyHeight) * 100.0), 12700));
        }
    }

    DrawCurve *current = nullptr;
    auto previousTick = INT_MIN;
    for (auto it = samples.cbegin(); it != samples.cend(); ++it) {
        if (!current || it.key() != previousTick + 5) {
            current = new DrawCurve;
            current->setLocalStart(it.key());
            result.append(current);
        }
        current->appendValue(it.value());
        previousTick = it.key();
    }
    clipController->onParamEdited(ParamInfo::Pitch, result);
}

void RhiPianoRollCanvas::startLyricEditing(const int noteId) {
    auto *note = findNote(noteId);
    if (!note)
        return;
    if (m_inlineEditor->isEditing())
        m_inlineEditor->submit();
    m_inlineEditingNoteId = noteId;
    m_inlineEditingPronunciation = false;

    const auto x = note->localStart() * ClipEditorGlobal::pixelsPerQuarterNote /
                   static_cast<double>(AppGlobal::ticksPerQuarterNote);
    const auto y = (127 - note->keyIndex()) * pianoKeyHeight;
    const auto width = qMax(40.0, note->length() * ClipEditorGlobal::pixelsPerQuarterNote /
                                      static_cast<double>(AppGlobal::ticksPerQuarterNote));
    const auto state = m_widget->viewportState();
    const auto cameraX =
        state.startTick * ClipEditorGlobal::pixelsPerQuarterNote / AppGlobal::ticksPerQuarterNote;
    const auto cameraY = (127.0 - state.topValue) * pianoKeyHeight;
    QRect anchor(qRound((x - cameraX) * m_widget->scaleX()),
                 qRound((y - cameraY) * m_widget->scaleY()), qRound(width * m_widget->scaleX()),
                 qRound(pianoKeyHeight * m_widget->scaleY()));
    const auto viewport = m_widget->rect().adjusted(0, 0, -14, -14);
    anchor.setWidth(qBound(40, anchor.width(), viewport.width()));
    anchor.setHeight(qBound(20, anchor.height(), viewport.height()));
    anchor.moveLeft(qBound(viewport.left(), anchor.left(), viewport.right() - anchor.width() + 1));
    anchor.moveTop(qBound(viewport.top(), anchor.top(), viewport.bottom() - anchor.height() + 1));
    QFont font;
    font.setPixelSize(12);
    m_inlineEditor->showAt(anchor, note->lyric(), font,
                           {
                               {QStringLiteral("editRole"), QStringLiteral("Lyric")}
    });
}

void RhiPianoRollCanvas::startPronunciationEditing(const int noteId) {
    auto *note = findNote(noteId);
    if (!note)
        return;
    if (m_inlineEditor->isEditing())
        m_inlineEditor->submit();
    m_inlineEditingNoteId = noteId;
    m_inlineEditingPronunciation = true;

    const auto x = note->localStart() * ClipEditorGlobal::pixelsPerQuarterNote /
                   static_cast<double>(AppGlobal::ticksPerQuarterNote);
    const auto y = (127 - note->keyIndex()) * pianoKeyHeight + pianoKeyHeight - 2.0;
    const auto width = qMax(40.0, note->length() * ClipEditorGlobal::pixelsPerQuarterNote /
                                      static_cast<double>(AppGlobal::ticksPerQuarterNote));
    const auto state = m_widget->viewportState();
    const auto cameraX =
        state.startTick * ClipEditorGlobal::pixelsPerQuarterNote / AppGlobal::ticksPerQuarterNote;
    const auto cameraY = (127.0 - state.topValue) * pianoKeyHeight;
    QRect anchor(qRound((x - cameraX) * m_widget->scaleX()),
                 qRound((y - cameraY) * m_widget->scaleY()), qRound(width * m_widget->scaleX()),
                 qMax(20, qRound(16.0 * m_widget->scaleY())));
    const auto viewport = m_widget->rect().adjusted(0, 0, -14, -14);
    anchor.setWidth(qBound(40, anchor.width(), viewport.width()));
    anchor.setHeight(qBound(20, anchor.height(), viewport.height()));
    anchor.moveLeft(qBound(viewport.left(), anchor.left(), viewport.right() - anchor.width() + 1));
    anchor.moveTop(qBound(viewport.top(), anchor.top(), viewport.bottom() - anchor.height() + 1));
    QFont font;
    font.setPixelSize(10);
    const auto pronunciation = note->pronunciation();
    const auto text = pronunciation.isEdited() ? pronunciation.edited : pronunciation.original;
    m_inlineEditor->showAt(anchor, text, font,
                           {
                               {QStringLiteral("editRole"), QStringLiteral("Pronunciation")}
    });
}

void RhiPianoRollCanvas::finishInlineEditing(const QString &text) {
    const auto noteId = m_inlineEditingNoteId;
    const auto pronunciationField = m_inlineEditingPronunciation;
    m_inlineEditingNoteId = -1;
    m_inlineEditingPronunciation = false;
    auto *note = findNote(noteId);
    if (!note)
        return;
    if (pronunciationField) {
        const auto pronunciation = note->pronunciation();
        const auto display =
            pronunciation.isEdited() ? pronunciation.edited : pronunciation.original;
        const auto edited = text.trimmed();
        if (edited != display && edited != pronunciation.edited)
            clipController->onNotePronunciationEdited(noteId, edited);
        return;
    }
    auto lyric = text.trimmed();
    if (lyric.isEmpty())
        lyric = appOptions->general()->defaultLyricForLanguage(note->language());
    if (lyric != note->lyric())
        clipController->onNoteLyricEdited(noteId, lyric);
}

void RhiPianoRollCanvas::openPhonemeEditor(const int noteId) {
    auto *note = findNote(noteId);
    if (!note)
        return;
    auto *dialog = new PhonemeEditorDialog(note, m_widget);
    connect(dialog, &PhonemeEditorDialog::accepted, this, [dialog, noteId] {
        clipController->onNotePhonemesEdited(noteId, dialog->phonemeNames());
    });
    dialog->show();
}

void RhiPianoRollCanvas::clearPastePreview() {
    if (m_pastePreviewRects.isEmpty() && m_pastePreviewTexts.isEmpty())
        return;
    m_pastePreviewRects.clear();
    m_pastePreviewTexts.clear();
    refreshSnapshot(EditorDirtyDomain::Overlay);
}

std::pair<int, int> RhiPianoRollCanvas::anchorAt(const QPointF &position) const {
    if (!m_clip)
        return {-1, -1};
    const auto *pitch = m_clip->params.getParamByName(ParamInfo::Pitch);
    auto bestDistance = 8.0;
    std::pair<int, int> best{-1, -1};
    const auto &curves = pitch->curves(Param::Edited);
    for (qsizetype curveIndex = 0; curveIndex < curves.size(); ++curveIndex) {
        if (curves.at(curveIndex)->type() != Curve::Anchor)
            continue;
        const auto *curve = static_cast<const AnchorCurve *>(curves.at(curveIndex));
        const auto nodes = curve->nodes().toList();
        for (qsizetype nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
            const auto *node = nodes.at(nodeIndex);
            const QPointF logical(node->pos() * ClipEditorGlobal::pixelsPerQuarterNote /
                                      static_cast<double>(AppGlobal::ticksPerQuarterNote),
                                  (12700 - node->value() + 50) * pianoKeyHeight / 100.0);
            const auto distance = std::hypot((logical.x() - position.x()) * m_widget->scaleX(),
                                             (logical.y() - position.y()) * m_widget->scaleY());
            if (distance < bestDistance) {
                bestDistance = distance;
                best = {static_cast<int>(curveIndex), static_cast<int>(nodeIndex)};
            }
        }
    }
    return best;
}

bool RhiPianoRollCanvas::isAnchorSelected(const int curveIndex, const int nodeIndex) const {
    return std::ranges::any_of(m_selectedAnchors, [curveIndex, nodeIndex](const auto &selection) {
        return selection.curveIndex == curveIndex && selection.nodeIndex == nodeIndex;
    });
}

void RhiPianoRollCanvas::selectAnchor(const int curveIndex, const int nodeIndex) {
    if (!m_clip)
        return;
    const auto *pitch = m_clip->params.getParamByName(ParamInfo::Pitch);
    const auto &curves = pitch->curves(Param::Edited);
    if (curveIndex < 0 || curveIndex >= curves.size() ||
        curves.at(curveIndex)->type() != Curve::Anchor)
        return;
    const auto nodes = static_cast<const AnchorCurve *>(curves.at(curveIndex))->nodes().toList();
    if (nodeIndex < 0 || nodeIndex >= nodes.size())
        return;
    const auto *node = nodes.at(nodeIndex);
    m_selectedAnchors = {
        {
         .curveIndex = curveIndex,
         .nodeIndex = nodeIndex,
         .startTick = node->pos(),
         .startValue = node->value(),
         }
    };
    m_anchorMergeCurveIndex = -1;
    m_anchorMergeNodeIndex = -1;
}

void RhiPianoRollCanvas::selectAnchorsInRect(const QRectF &rect, const bool additive) {
    if (!m_clip)
        return;
    const auto *pitch = m_clip->params.getParamByName(ParamInfo::Pitch);
    const auto &curves = pitch->curves(Param::Edited);
    auto selected = rect.isNull()
                        ? m_selectedAnchors
                        : (additive ? m_anchorSelectionBeforeRect : QVector<AnchorSelection>{});

    QVector<AnchorSelection> refreshed;
    for (const auto &selection : selected) {
        if (selection.curveIndex < 0 || selection.curveIndex >= curves.size() ||
            curves.at(selection.curveIndex)->type() != Curve::Anchor)
            continue;
        const auto nodes =
            static_cast<const AnchorCurve *>(curves.at(selection.curveIndex))->nodes().toList();
        if (selection.nodeIndex < 0 || selection.nodeIndex >= nodes.size())
            continue;
        const auto *node = nodes.at(selection.nodeIndex);
        refreshed.append({
            .curveIndex = selection.curveIndex,
            .nodeIndex = selection.nodeIndex,
            .startTick = node->pos(),
            .startValue = node->value(),
        });
    }

    if (!rect.isNull()) {
        for (qsizetype curveIndex = 0; curveIndex < curves.size(); ++curveIndex) {
            if (curves.at(curveIndex)->type() != Curve::Anchor)
                continue;
            const auto nodes =
                static_cast<const AnchorCurve *>(curves.at(curveIndex))->nodes().toList();
            for (qsizetype nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
                const auto *node = nodes.at(nodeIndex);
                const auto x = node->pos() * ClipEditorGlobal::pixelsPerQuarterNote /
                               static_cast<double>(AppGlobal::ticksPerQuarterNote);
                const auto y = (12700 - node->value() + 50) * pianoKeyHeight / 100.0;
                if (!rect.contains(x, y) ||
                    std::ranges::any_of(refreshed, [curveIndex, nodeIndex](const auto &selection) {
                        return selection.curveIndex == curveIndex &&
                               selection.nodeIndex == nodeIndex;
                    }))
                    continue;
                refreshed.append({
                    .curveIndex = static_cast<int>(curveIndex),
                    .nodeIndex = static_cast<int>(nodeIndex),
                    .startTick = node->pos(),
                    .startValue = node->value(),
                });
            }
        }
    }
    m_selectedAnchors = refreshed;
}

void RhiPianoRollCanvas::updateAnchorMergeCandidate(const QPointF &position) {
    m_anchorMergeCurveIndex = -1;
    m_anchorMergeNodeIndex = -1;
    if (!m_clip || m_selectedAnchors.isEmpty())
        return;
    const auto [curveIndex, nodeIndex] = anchorAt(position);
    const auto sourceCurveIndex = m_selectedAnchors.constFirst().curveIndex;
    if (!std::ranges::all_of(m_selectedAnchors, [sourceCurveIndex](const auto &selection) {
            return selection.curveIndex == sourceCurveIndex;
        }))
        return;
    if (curveIndex < 0 || curveIndex == sourceCurveIndex)
        return;

    const auto *pitch = m_clip->params.getParamByName(ParamInfo::Pitch);
    const auto &curves = pitch->curves(Param::Edited);
    if (sourceCurveIndex < 0 || sourceCurveIndex >= curves.size() || curveIndex >= curves.size() ||
        curves.at(sourceCurveIndex)->type() != Curve::Anchor ||
        curves.at(curveIndex)->type() != Curve::Anchor)
        return;
    const auto sourceNodes =
        static_cast<const AnchorCurve *>(curves.at(sourceCurveIndex))->nodes().toList();
    const auto candidateNodes =
        static_cast<const AnchorCurve *>(curves.at(curveIndex))->nodes().toList();
    if (sourceNodes.isEmpty() || candidateNodes.isEmpty() ||
        (nodeIndex != 0 && nodeIndex != candidateNodes.size() - 1))
        return;
    const auto separated = candidateNodes.last()->pos() < sourceNodes.first()->pos() ||
                           candidateNodes.first()->pos() > sourceNodes.last()->pos();
    if (!separated)
        return;
    m_anchorMergeCurveIndex = curveIndex;
    m_anchorMergeNodeIndex = nodeIndex;
}

void RhiPianoRollCanvas::commitAnchorMove(const bool removeSelected) {
    if (!m_clip || m_selectedAnchors.isEmpty())
        return;

    struct AnchorData {
        int tick = 0;
        int value = 0;
        AnchorNode::InterpMode interpolation = AnchorNode::Hermite;
        bool selected = false;
    };

    const auto *pitch = m_clip->params.getParamByName(ParamInfo::Pitch);
    const auto &curves = pitch->curves(Param::Edited);
    QHash<int, QPair<int, int>> curveRanges;
    for (qsizetype curveIndex = 0; curveIndex < curves.size(); ++curveIndex) {
        if (curves.at(curveIndex)->type() != Curve::Anchor)
            continue;
        const auto nodes =
            static_cast<const AnchorCurve *>(curves.at(curveIndex))->nodes().toList();
        if (!nodes.isEmpty())
            curveRanges.insert(static_cast<int>(curveIndex),
                               {nodes.first()->pos(), nodes.last()->pos()});
    }

    QVector<QPair<int, AnchorData>> pending;
    for (qsizetype curveIndex = 0; curveIndex < curves.size(); ++curveIndex) {
        if (curves.at(curveIndex)->type() != Curve::Anchor)
            continue;
        const auto nodes =
            static_cast<const AnchorCurve *>(curves.at(curveIndex))->nodes().toList();
        for (qsizetype nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
            const auto selected =
                isAnchorSelected(static_cast<int>(curveIndex), static_cast<int>(nodeIndex));
            if (selected && removeSelected)
                continue;
            const auto *source = nodes.at(nodeIndex);
            AnchorData data{
                .tick = source->pos() + (selected ? m_anchorPreviewDeltaTick : 0),
                .value = source->value() + (selected ? m_anchorPreviewDeltaValue : 0),
                .interpolation = source->interpMode(),
                .selected = selected,
            };
            auto targetCurveIndex = static_cast<int>(curveIndex);
            if (selected && !removeSelected) {
                for (auto it = curveRanges.cbegin(); it != curveRanges.cend(); ++it) {
                    if (it.key() != curveIndex && data.tick >= it.value().first &&
                        data.tick <= it.value().second) {
                        targetCurveIndex = it.key();
                        break;
                    }
                }
            }
            pending.append({targetCurveIndex, data});
        }
    }
    std::ranges::stable_sort(pending, [](const auto &left, const auto &right) {
        return left.second.selected < right.second.selected;
    });

    QHash<int, QMap<int, AnchorData>> nodesByCurve;
    for (const auto &[curveIndex, data] : pending)
        nodesByCurve[curveIndex].insert(data.tick, data);

    QList<Curve *> result;
    QVector<AnchorSelection> remappedSelection;
    for (qsizetype curveIndex = 0; curveIndex < curves.size(); ++curveIndex) {
        const auto *curve = curves.at(curveIndex);
        if (curve->type() == Curve::Draw) {
            result.append(new DrawCurve(*static_cast<const DrawCurve *>(curve)));
            continue;
        }
        if (curve->type() != Curve::Anchor)
            continue;
        const auto rebuiltNodes = nodesByCurve.value(static_cast<int>(curveIndex));
        if (rebuiltNodes.isEmpty())
            continue;
        auto *copy = new AnchorCurve;
        const auto resultCurveIndex = result.size();
        auto rebuiltNodeIndex = 0;
        for (auto it = rebuiltNodes.cbegin(); it != rebuiltNodes.cend(); ++it, ++rebuiltNodeIndex) {
            auto data = it.value();
            if (it == std::prev(rebuiltNodes.cend()))
                data.interpolation = AnchorNode::None;
            else if (data.interpolation == AnchorNode::None)
                data.interpolation = AnchorNode::Hermite;
            auto *node = new AnchorNode(data.tick, data.value);
            node->setInterpMode(data.interpolation);
            copy->insertNode(node);
            if (data.selected) {
                remappedSelection.append({
                    .curveIndex = static_cast<int>(resultCurveIndex),
                    .nodeIndex = rebuiltNodeIndex,
                    .startTick = data.tick,
                    .startValue = data.value,
                });
            }
        }
        result.append(copy);
    }
    clipController->onParamEdited(ParamInfo::Pitch, result);
    m_selectedAnchors = removeSelected ? QVector<AnchorSelection>{} : remappedSelection;
}

void RhiPianoRollCanvas::mergeAnchorCurves(const int sourceCurveIndex, const int targetCurveIndex) {
    if (!m_clip || sourceCurveIndex < 0 || targetCurveIndex < 0 ||
        sourceCurveIndex == targetCurveIndex)
        return;
    const auto *pitch = m_clip->params.getParamByName(ParamInfo::Pitch);
    const auto &curves = pitch->curves(Param::Edited);
    if (sourceCurveIndex >= curves.size() || targetCurveIndex >= curves.size() ||
        curves.at(sourceCurveIndex)->type() != Curve::Anchor ||
        curves.at(targetCurveIndex)->type() != Curve::Anchor)
        return;

    QList<Curve *> result;
    for (qsizetype curveIndex = 0; curveIndex < curves.size(); ++curveIndex) {
        const auto *curve = curves.at(curveIndex);
        if (curve->type() == Curve::Draw) {
            result.append(new DrawCurve(*static_cast<const DrawCurve *>(curve)));
        } else if (curve->type() != Curve::Anchor) {
            continue;
        } else if (curveIndex == targetCurveIndex) {
            continue;
        } else if (curveIndex != sourceCurveIndex) {
            result.append(new AnchorCurve(*static_cast<const AnchorCurve *>(curve)));
        } else {
            QMap<int, QPair<int, AnchorNode::InterpMode>> merged;
            for (const auto index : {sourceCurveIndex, targetCurveIndex}) {
                const auto nodes =
                    static_cast<const AnchorCurve *>(curves.at(index))->nodes().toList();
                for (const auto *node : nodes)
                    merged.insert(node->pos(), {node->value(), node->interpMode()});
            }
            auto *copy = new AnchorCurve;
            for (auto it = merged.cbegin(); it != merged.cend(); ++it) {
                auto *node = new AnchorNode(it.key(), it.value().first);
                auto mode = it.value().second;
                if (it == std::prev(merged.cend()))
                    mode = AnchorNode::None;
                else if (mode == AnchorNode::None)
                    mode = AnchorNode::Hermite;
                node->setInterpMode(mode);
                copy->insertNode(node);
            }
            result.append(copy);
        }
    }
    clipController->onParamEdited(ParamInfo::Pitch, result);
}

void RhiPianoRollCanvas::setSelectedAnchorInterpolation(const int interpolationMode) {
    if (!m_clip || m_selectedAnchors.isEmpty())
        return;
    const auto *pitch = m_clip->params.getParamByName(ParamInfo::Pitch);
    const auto &curves = pitch->curves(Param::Edited);
    QList<Curve *> result;
    for (qsizetype curveIndex = 0; curveIndex < curves.size(); ++curveIndex) {
        const auto *curve = curves.at(curveIndex);
        if (curve->type() == Curve::Draw) {
            result.append(new DrawCurve(*static_cast<const DrawCurve *>(curve)));
            continue;
        }
        if (curve->type() != Curve::Anchor)
            continue;
        auto *copy = new AnchorCurve(*static_cast<const AnchorCurve *>(curve));
        const auto nodes = copy->nodes().toList();
        for (qsizetype nodeIndex = 0; nodeIndex + 1 < nodes.size(); ++nodeIndex) {
            if (isAnchorSelected(static_cast<int>(curveIndex), static_cast<int>(nodeIndex))) {
                nodes.at(nodeIndex)->setInterpMode(
                    static_cast<AnchorNode::InterpMode>(interpolationMode));
            }
        }
        result.append(copy);
    }
    beginPitchTransaction();
    clipController->onParamEdited(ParamInfo::Pitch, result);
    finishEditTransaction(true);
    selectAnchorsInRect({}, true);
    refreshSnapshot(EditorDirtyDomain::All);
}

void RhiPianoRollCanvas::insertAnchorAt(const QPointF &position) {
    if (!m_clip)
        return;
    const auto tick = qMax(0, localTickAt(position.x()));
    const auto value = qBound(0, qRound((127.5 - position.y() / pianoKeyHeight) * 100.0), 12700);
    const auto *pitch = m_clip->params.getParamByName(ParamInfo::Pitch);
    for (const auto *curve : pitch->curves(Param::Edited)) {
        if (curve->type() != Curve::Anchor)
            continue;
        for (const auto *node : static_cast<const AnchorCurve *>(curve)->nodes().toList()) {
            if (node->pos() == tick)
                return;
        }
    }
    const auto &editedCurves = pitch->curves(Param::Edited);
    auto targetCurveIndex = -1;
    for (qsizetype curveIndex = 0; curveIndex < editedCurves.size(); ++curveIndex) {
        if (editedCurves.at(curveIndex)->type() != Curve::Anchor)
            continue;
        const auto nodes =
            static_cast<const AnchorCurve *>(editedCurves.at(curveIndex))->nodes().toList();
        if (!nodes.isEmpty() && tick >= nodes.first()->pos() && tick <= nodes.last()->pos()) {
            targetCurveIndex = static_cast<int>(curveIndex);
            break;
        }
    }
    if (targetCurveIndex < 0 && !m_selectedAnchors.isEmpty()) {
        const auto sourceCurveIndex = m_selectedAnchors.constFirst().curveIndex;
        if (sourceCurveIndex >= 0 && sourceCurveIndex < editedCurves.size() &&
            editedCurves.at(sourceCurveIndex)->type() == Curve::Anchor) {
            const auto sourceNodes =
                static_cast<const AnchorCurve *>(editedCurves.at(sourceCurveIndex))
                    ->nodes()
                    .toList();
            auto minimum = 0;
            auto maximum = INT_MAX;
            if (!sourceNodes.isEmpty()) {
                for (qsizetype curveIndex = 0; curveIndex < editedCurves.size(); ++curveIndex) {
                    if (curveIndex == sourceCurveIndex ||
                        editedCurves.at(curveIndex)->type() != Curve::Anchor)
                        continue;
                    const auto otherNodes =
                        static_cast<const AnchorCurve *>(editedCurves.at(curveIndex))
                            ->nodes()
                            .toList();
                    if (otherNodes.isEmpty())
                        continue;
                    if (otherNodes.last()->pos() < sourceNodes.first()->pos())
                        minimum = qMax(minimum, otherNodes.last()->pos() + 1);
                    if (otherNodes.first()->pos() > sourceNodes.last()->pos())
                        maximum = qMin(maximum, otherNodes.first()->pos() - 1);
                }
                if (tick >= minimum && tick <= maximum)
                    targetCurveIndex = sourceCurveIndex;
            }
        }
    }

    QList<Curve *> result;
    AnchorCurve *target = nullptr;
    for (qsizetype curveIndex = 0; curveIndex < editedCurves.size(); ++curveIndex) {
        const auto *curve = editedCurves.at(curveIndex);
        if (curve->type() == Curve::Draw) {
            result.append(new DrawCurve(*static_cast<const DrawCurve *>(curve)));
            continue;
        }
        if (curve->type() != Curve::Anchor)
            continue;
        auto *copy = new AnchorCurve(*static_cast<const AnchorCurve *>(curve));
        if (curveIndex == targetCurveIndex)
            target = copy;
        result.append(copy);
    }
    if (!target) {
        target = new AnchorCurve;
        result.append(target);
    }
    const auto existingNodes = target->nodes().toList();
    auto *node = new AnchorNode(tick, value);
    if (existingNodes.isEmpty() || tick > existingNodes.last()->pos()) {
        node->setInterpMode(AnchorNode::None);
        if (!existingNodes.isEmpty()) {
            auto predecessorMode = existingNodes.last()->interpMode();
            if (predecessorMode == AnchorNode::None) {
                predecessorMode = existingNodes.size() > 1
                                      ? existingNodes.at(existingNodes.size() - 2)->interpMode()
                                      : AnchorNode::Hermite;
            }
            existingNodes.last()->setInterpMode(predecessorMode);
        }
    } else {
        auto interpolationMode = AnchorNode::Hermite;
        for (auto index = existingNodes.size() - 1; index >= 0; --index) {
            if (existingNodes.at(index)->pos() < tick) {
                interpolationMode = existingNodes.at(index)->interpMode();
                break;
            }
        }
        node->setInterpMode(interpolationMode);
    }
    target->insertNode(node);
    beginPitchTransaction();
    clipController->onParamEdited(ParamInfo::Pitch, result);
    finishEditTransaction(true);
    m_selectedAnchors.clear();
    const auto &updatedCurves =
        m_clip->params.getParamByName(ParamInfo::Pitch)->curves(Param::Edited);
    for (qsizetype curveIndex = 0; curveIndex < updatedCurves.size(); ++curveIndex) {
        if (updatedCurves.at(curveIndex)->type() != Curve::Anchor)
            continue;
        const auto nodes =
            static_cast<const AnchorCurve *>(updatedCurves.at(curveIndex))->nodes().toList();
        for (qsizetype nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
            if (nodes.at(nodeIndex)->pos() == tick) {
                selectAnchor(static_cast<int>(curveIndex), static_cast<int>(nodeIndex));
                return;
            }
        }
    }
}

void RhiPianoRollCanvas::beginEditTransaction(const QList<int> &noteIds) {
    if (!m_clip || editSessionManager->hasActiveTransaction())
        return;
    editSessionManager->beginTransaction(AppStatus::EditObjectType::Note, m_clip->id(), {}, noteIds,
                                         {}, {}, true);
    appStatus->currentEditObject = AppStatus::EditObjectType::Note;
    m_editTransactionActive = true;
}

void RhiPianoRollCanvas::finishEditTransaction(const bool commit) {
    if (!m_editTransactionActive)
        return;
    editSessionManager->endActiveTransaction(commit ? EditSessionEndReason::Commit
                                                    : EditSessionEndReason::Discard);
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
    m_editTransactionActive = false;
}

void RhiPianoRollCanvas::positionViewportAtClipContent() {
    if (!m_clip)
        return;

    const auto tickRange = qMax(1.0, m_widget->endTick() - m_widget->startTick());
    if (m_clip->notes().count() > 0) {
        const auto *firstNote = *m_clip->notes().begin();
        m_widget->setViewportCenter(firstNote->localStart() + tickRange * 0.2,
                                    firstNote->keyIndex());
        return;
    }
    m_widget->setViewportCenter(tickRange * 0.5, 60.0);
}

Note *RhiPianoRollCanvas::findNote(const int id) const {
    return m_clip && id >= 0 ? m_clip->findNoteById(id) : nullptr;
}

int RhiPianoRollCanvas::localTickAt(const double x) const {
    return qRound(x * AppGlobal::ticksPerQuarterNote / ClipEditorGlobal::pixelsPerQuarterNote);
}

int RhiPianoRollCanvas::keyAt(const double y) const {
    return qBound(0, 127 - static_cast<int>(std::floor(y / pianoKeyHeight)), 127);
}

int RhiPianoRollCanvas::snappedLocalTick(const int tick, const bool nearest) const {
    const auto step = TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
    const auto globalTick = tick + clipOffset();
    const auto snapped =
        nearest ? TimelineSnapUtils::snapNearest(globalTick, step, appModel->timeline())
                : TimelineSnapUtils::snapDown(globalTick, step, appModel->timeline());
    return snapped - clipOffset();
}

int RhiPianoRollCanvas::clipOffset() const {
    return m_clip ? m_clip->start() : 0;
}

ImmutableEditorRenderSnapshot RhiPianoRollCanvas::buildSnapshot() {
    auto snapshot = std::make_shared<EditorRenderSnapshot>();
    snapshot->kind = EditorCanvasKind::PianoRoll;
    snapshot->revision = m_revision + 1;
    snapshot->logicalExtent = {
        (m_clip ? m_clip->length() : AppGlobal::ticksPerWholeNote * 80.0) *
            ClipEditorGlobal::pixelsPerQuarterNote / AppGlobal::ticksPerQuarterNote,
        128.0 * pianoKeyHeight,
    };
    snapshot->selectedIds = appStatus->selectedNotes;
    snapshot->hoveredId = m_hoveredNoteId;
    const auto viewport = m_widget->viewportState();
    const auto viewportTickSpan = qMax(static_cast<double>(AppGlobal::ticksPerWholeNote * 2),
                                       viewport.endTick - viewport.startTick);
    m_cachedStartTick = qMax(0.0, viewport.startTick - viewportTickSpan);
    m_cachedEndTick = viewport.endTick + viewportTickSpan;
    const auto viewportKeySpan = qMax(12.0, viewport.topValue - viewport.bottomValue);
    m_cachedTopKey = qMin(127.0, viewport.topValue + viewportKeySpan);
    m_cachedBottomKey = qMax(0.0, viewport.bottomValue - viewportKeySpan);

    for (int key = 0; key < 128; ++key) {
        if (key > m_cachedTopKey || key < m_cachedBottomKey)
            continue;
        const auto pitchClass = key % 12;
        const auto black = pitchClass == 1 || pitchClass == 3 || pitchClass == 6 ||
                           pitchClass == 8 || pitchClass == 10;
        const auto y = (127 - key) * pianoKeyHeight;
        snapshot->rectangles.append({
            .objectId = -1,
            .bounds = {0.0, y, snapshot->logicalExtent.width(), pianoKeyHeight},
            .fill = black ? QColor(27, 30, 36) : QColor(34, 38, 45),
            .border = pitchClass == 0 ? QColor(72, 78, 90) : QColor(49, 54, 63),
        });
        if (pitchClass == 0) {
            QFont octaveFont;
            octaveFont.setPixelSize(10);
            snapshot->texts.append({
                .text = QStringLiteral("C%1").arg(key / 12 - 1),
                .baseline = {4.0, y + 12.0},
                .color = QColor(145, 150, 162),
                .font = octaveFont,
                .clip = {0.0, y, 40.0, pianoKeyHeight},
                .layer = 1,
            });
        }
    }

    const auto quarterWidth = static_cast<double>(ClipEditorGlobal::pixelsPerQuarterNote);
    const auto cachedStartX = m_cachedStartTick * quarterWidth / AppGlobal::ticksPerQuarterNote;
    const auto cachedEndX = m_cachedEndTick * quarterWidth / AppGlobal::ticksPerQuarterNote;
    const auto firstGridX = std::floor(cachedStartX / quarterWidth) * quarterWidth;
    const auto lastGridX = qMin(snapshot->logicalExtent.width(), cachedEndX);
    for (double x = firstGridX; x <= lastGridX; x += quarterWidth) {
        const auto quarter = qRound64(x / quarterWidth);
        snapshot->lines.append({
            {x, 0.0                             },
            {x, snapshot->logicalExtent.height()},
            quarter % 4 == 0 ? QColor(77, 83, 96) : QColor(52, 57, 67),
            1.0F,
            1
        });
    }

    if (!m_clip)
        return snapshot;

    const auto clipLeft = m_clip->clipStart() * quarterWidth / AppGlobal::ticksPerQuarterNote;
    const auto clipRight =
        (m_clip->clipStart() + m_clip->clipLen()) * quarterWidth / AppGlobal::ticksPerQuarterNote;
    const auto visibleTop = (127.0 - m_cachedTopKey) * pianoKeyHeight;
    const auto visibleBottom = (128.0 - m_cachedBottomKey) * pianoKeyHeight;
    const auto maskColor = QColor(8, 10, 14, 150);
    if (clipLeft > cachedStartX) {
        snapshot->rectangles.append({
            .objectId = -1,
            .bounds = {cachedStartX, visibleTop, clipLeft - cachedStartX,
                       visibleBottom - visibleTop},
            .fill = maskColor,
            .layer = 2,
        });
    }
    if (clipRight < cachedEndX) {
        snapshot->rectangles.append({
            .objectId = -1,
            .bounds = {clipRight, visibleTop, cachedEndX - clipRight, visibleBottom - visibleTop},
            .fill = maskColor,
            .layer = 2,
        });
    }

    const auto palette = AppColorPalette::instance();
    for (const auto *note : m_clip->notes()) {
        auto start = note->localStart();
        auto length = note->length();
        auto key = note->keyIndex();
        const auto selected = snapshot->selectedIds.contains(note->id());
        if (m_interaction == Interaction::Move && selected) {
            start += m_previewDeltaTick;
            key += m_previewDeltaKey;
        } else if (note->id() == m_pressedNoteId && m_interaction == Interaction::ResizeLeft) {
            start += m_previewDeltaTick;
            length -= m_previewDeltaTick;
        } else if (note->id() == m_pressedNoteId && m_interaction == Interaction::ResizeRight) {
            length += m_previewDeltaTick;
        }
        if (start + length < m_cachedStartTick || start > m_cachedEndTick || key > m_cachedTopKey ||
            key < m_cachedBottomKey)
            continue;
        const auto x =
            start * ClipEditorGlobal::pixelsPerQuarterNote / AppGlobal::ticksPerQuarterNote;
        const auto width = qMax(1.0, length * ClipEditorGlobal::pixelsPerQuarterNote /
                                         static_cast<double>(AppGlobal::ticksPerQuarterNote));
        const auto y = (127 - key) * pianoKeyHeight + 1.5;
        EditorRenderRect noteRect;
        noteRect.objectId = note->id();
        noteRect.bounds = QRectF(x, y, width, pianoKeyHeight - 3.0);
        if (m_eraseNoteIds.contains(note->id())) {
            noteRect.fill = QColor(220, 76, 76, 110);
        } else if (note->overlapped()) {
            noteRect.fill = palette->noteBackgroundOverlapped(m_trackColorIndex);
        } else {
            noteRect.fill = selected ? palette->noteBackgroundSelected(m_trackColorIndex)
                                     : palette->noteBackground(m_trackColorIndex);
        }
        noteRect.border =
            selected || note->id() == m_hoveredNoteId
                ? QColor(240, 242, 248)
                : (note->overlapped() ? palette->noteBorderOverlapped(m_trackColorIndex)
                                      : palette->noteBorder(m_trackColorIndex));
        noteRect.selected = selected;
        noteRect.hovered = note->id() == m_hoveredNoteId;
        noteRect.layer = 10;
        snapshot->rectangles.append(noteRect);

        QFont noteFont;
        noteFont.setPixelSize(12);
        snapshot->texts.append({
            .text = note->lyric(),
            .baseline = {noteRect.bounds.left() + 4.0, noteRect.bounds.top() + 15.0},
            .color = palette->noteForeground(m_trackColorIndex),
            .font = noteFont,
            .clip = noteRect.bounds.adjusted(2.0, 1.0, -2.0, -1.0),
            .layer = 12,
        });
        const auto pronunciation = note->pronunciation();
        const auto pronunciationText =
            pronunciation.isEdited() ? pronunciation.edited : pronunciation.original;
        if (!pronunciationText.isEmpty()) {
            QFont pronunciationFont;
            pronunciationFont.setPixelSize(10);
            snapshot->texts.append({
                .text = pronunciationText,
                .baseline = {noteRect.bounds.left() + 3.0, noteRect.bounds.bottom() + 11.0},
                .color = pronunciation.isEdited() ? palette->phonemeEdited(m_trackColorIndex)
                                                  : QColor(200, 200, 200),
                .font = pronunciationFont,
                .clip = {noteRect.bounds.left(), noteRect.bounds.bottom(), noteRect.bounds.width(),
                             14.0},
                .layer = 12,
            });
        }
    }

    const auto appendPitchCurves = [this, snapshot](const QList<Curve *> &curves,
                                                    const QColor &color) {
        for (const auto *curve : curves) {
            std::unique_ptr<DrawCurve> converted;
            const DrawCurve *drawCurve = nullptr;
            if (curve->type() == Curve::Draw) {
                drawCurve = static_cast<const DrawCurve *>(curve);
            } else if (curve->type() == Curve::Anchor) {
                converted.reset(static_cast<const AnchorCurve *>(curve)->toDrawCurve());
                drawCurve = converted.get();
            }
            if (!drawCurve)
                continue;
            const auto &values = drawCurve->values();
            EditorRenderPath path;
            path.color = color;
            path.width = 1.5F;
            path.join = EditorStrokeJoin::Round;
            path.cap = EditorStrokeCap::Round;
            path.layer = 20;
            for (qsizetype i = 0; i < values.size(); ++i) {
                const auto tick = drawCurve->localStart() + i * drawCurve->step;
                if (tick < m_cachedStartTick - drawCurve->step ||
                    tick > m_cachedEndTick + drawCurve->step)
                    continue;
                const auto x = tick * ClipEditorGlobal::pixelsPerQuarterNote /
                               static_cast<double>(AppGlobal::ticksPerQuarterNote);
                const auto y =
                    (12700 - qBound(0, values.at(i), 12700) + 50) * pianoKeyHeight / 100.0;
                path.points.append({x, y});
            }
            if (path.points.size() >= 2)
                snapshot->paths.append(path);
        }
    };
    const auto *pitch = m_clip->params.getParamByName(ParamInfo::Pitch);
    appendPitchCurves(pitch->curves(Param::Original), QColor(130, 145, 170, 150));
    QList<Curve *> previewCurves;
    const auto &editedPitchCurves = pitch->curves(Param::Edited);
    if (m_interaction == Interaction::MoveAnchor) {
        for (qsizetype curveIndex = 0; curveIndex < editedPitchCurves.size(); ++curveIndex) {
            const auto *curve = editedPitchCurves.at(curveIndex);
            if (curve->type() == Curve::Draw) {
                previewCurves.append(new DrawCurve(*static_cast<const DrawCurve *>(curve)));
                continue;
            }
            if (curve->type() != Curve::Anchor)
                continue;
            auto *previewCurve = new AnchorCurve;
            const auto nodes = static_cast<const AnchorCurve *>(curve)->nodes().toList();
            for (qsizetype nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
                const auto *source = nodes.at(nodeIndex);
                const auto selected =
                    isAnchorSelected(static_cast<int>(curveIndex), static_cast<int>(nodeIndex));
                auto *node =
                    new AnchorNode(source->pos() + (selected ? m_anchorPreviewDeltaTick : 0),
                                   source->value() + (selected ? m_anchorPreviewDeltaValue : 0));
                node->setInterpMode(source->interpMode());
                previewCurve->insertNode(node);
            }
            previewCurves.append(previewCurve);
        }
        appendPitchCurves(previewCurves, QColor(105, 190, 255, 230));
    } else {
        appendPitchCurves(editedPitchCurves, QColor(105, 190, 255, 230));
    }
    const auto &editedCurves = pitch->curves(Param::Edited);
    for (qsizetype curveIndex = 0; curveIndex < editedCurves.size(); ++curveIndex) {
        if (editedCurves.at(curveIndex)->type() != Curve::Anchor)
            continue;
        const auto *curve = static_cast<const AnchorCurve *>(editedCurves.at(curveIndex));
        const auto nodes = curve->nodes().toList();
        for (qsizetype nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
            const auto *node = nodes.at(nodeIndex);
            auto tick = node->pos();
            auto value = node->value();
            const auto selected =
                isAnchorSelected(static_cast<int>(curveIndex), static_cast<int>(nodeIndex));
            if (selected && m_interaction == Interaction::MoveAnchor) {
                tick += m_anchorPreviewDeltaTick;
                value += m_anchorPreviewDeltaValue;
            }
            const auto x = tick * ClipEditorGlobal::pixelsPerQuarterNote /
                           static_cast<double>(AppGlobal::ticksPerQuarterNote);
            const auto y = (12700 - value + 50) * pianoKeyHeight / 100.0;
            const auto mergeCandidate =
                curveIndex == m_anchorMergeCurveIndex && nodeIndex == m_anchorMergeNodeIndex;
            const auto radius = selected || mergeCandidate ? 5.0 : 3.0;
            snapshot->rectangles.append({
                .objectId = -1,
                .bounds = {x - radius, y - radius, radius * 2.0, radius * 2.0},
                .fill = mergeCandidate ? QColor(105, 235, 170)
                                       : (selected ? QColor(255, 210, 110) : QColor(125, 205, 255)),
                .border =
                    selected || mergeCandidate ? QColor(255, 245, 210) : QColor(210, 240, 255),
                .layer = 21,
            });
        }
    }
    if (m_anchorMergeCurveIndex >= 0 && !m_selectedAnchors.isEmpty() &&
        m_anchorMergeCurveIndex < editedCurves.size() &&
        editedCurves.at(m_anchorMergeCurveIndex)->type() == Curve::Anchor) {
        const auto sourceCurveIndex = m_selectedAnchors.constFirst().curveIndex;
        if (sourceCurveIndex >= 0 && sourceCurveIndex < editedCurves.size() &&
            editedCurves.at(sourceCurveIndex)->type() == Curve::Anchor) {
            const auto sourceNodes =
                static_cast<const AnchorCurve *>(editedCurves.at(sourceCurveIndex))
                    ->nodes()
                    .toList();
            const auto targetNodes =
                static_cast<const AnchorCurve *>(editedCurves.at(m_anchorMergeCurveIndex))
                    ->nodes()
                    .toList();
            if (!sourceNodes.isEmpty() && m_anchorMergeNodeIndex >= 0 &&
                m_anchorMergeNodeIndex < targetNodes.size()) {
                const auto *targetNode = targetNodes.at(m_anchorMergeNodeIndex);
                const auto *sourceNode = targetNode->pos() < sourceNodes.first()->pos()
                                             ? sourceNodes.first()
                                             : sourceNodes.last();
                const auto pointForNode = [](const AnchorNode *node) {
                    return QPointF(node->pos() * ClipEditorGlobal::pixelsPerQuarterNote /
                                       static_cast<double>(AppGlobal::ticksPerQuarterNote),
                                   (12700 - node->value() + 50) * pianoKeyHeight / 100.0);
                };
                snapshot->lines.append({
                    pointForNode(sourceNode),
                    pointForNode(targetNode),
                    QColor(105, 235, 170, 190),
                    1.5F,
                    21,
                });
            }
        }
    }
    if (m_editMode == ClipEditorGlobal::EditPitchAnchor && m_pointerInside &&
        m_interaction == Interaction::None && !m_selectedAnchors.isEmpty() &&
        m_anchorMergeCurveIndex < 0 && anchorAt(m_lastPosition).first < 0) {
        const auto tick = qMax(0, localTickAt(m_lastPosition.x()));
        const auto value =
            qBound(0, qRound((127.5 - m_lastPosition.y() / pianoKeyHeight) * 100.0), 12700);
        const auto x = tick * ClipEditorGlobal::pixelsPerQuarterNote /
                       static_cast<double>(AppGlobal::ticksPerQuarterNote);
        const auto y = (12700 - value + 50) * pianoKeyHeight / 100.0;
        snapshot->rectangles.append({
            .objectId = -1,
            .bounds = {x - 4.0, y - 4.0, 8.0, 8.0},
            .fill = QColor(105, 190, 255, 130),
            .border = QColor(190, 225, 255, 190),
            .layer = 21,
        });
    }
    qDeleteAll(previewCurves);
    if (m_interaction == Interaction::Draw) {
        const auto x = m_drawStart * ClipEditorGlobal::pixelsPerQuarterNote /
                       static_cast<double>(AppGlobal::ticksPerQuarterNote);
        const auto width = m_drawLength * ClipEditorGlobal::pixelsPerQuarterNote /
                           static_cast<double>(AppGlobal::ticksPerQuarterNote);
        EditorRenderRect drawRect;
        drawRect.bounds =
            QRectF(x, (127 - m_drawKey) * pianoKeyHeight + 1.5, width, pianoKeyHeight - 3.0);
        drawRect.fill = QColor(110, 170, 255, 150);
        drawRect.border = QColor(225, 235, 255);
        drawRect.layer = 30;
        snapshot->rectangles.append(drawRect);
    }
    if (m_interaction == Interaction::RectSelect && !m_selectionRect.isNull()) {
        EditorRenderRect selectionRect;
        selectionRect.bounds = m_selectionRect;
        selectionRect.fill = QColor(90, 145, 230, 45);
        selectionRect.border = m_editMode == ClipEditorGlobal::IntervalSelect
                                   ? QColor(Qt::transparent)
                                   : QColor(120, 175, 255, 210);
        selectionRect.layer = 30;
        snapshot->rectangles.append(selectionRect);
        if (m_editMode == ClipEditorGlobal::IntervalSelect) {
            snapshot->lines.append({
                m_selectionRect.topLeft(),
                m_selectionRect.bottomLeft(),
                QColor(120, 175, 255, 210),
                1.5F,
                30,
            });
            snapshot->lines.append({
                m_selectionRect.topRight(),
                m_selectionRect.bottomRight(),
                QColor(120, 175, 255, 210),
                1.5F,
                30,
            });
        }
    }
    if ((m_interaction == Interaction::DrawPitch || m_interaction == Interaction::ErasePitch) &&
        m_pitchStroke.size() >= 2) {
        snapshot->paths.append({
            .points = m_pitchStroke,
            .color = m_interaction == Interaction::DrawPitch ? QColor(100, 205, 255, 220)
                                                             : QColor(255, 100, 100, 210),
            .width = m_interaction == Interaction::DrawPitch ? 2.0F : 8.0F,
            .join = EditorStrokeJoin::Round,
            .cap = EditorStrokeCap::Round,
            .layer = 30,
        });
    }
    if (m_editMode == ClipEditorGlobal::SplitNote && m_hoveredNoteId >= 0) {
        if (const auto *note = findNote(m_hoveredNoteId)) {
            const auto tick = snappedLocalTick(localTickAt(m_lastPosition.x()), true);
            if (tick > note->localStart() && tick < note->localStart() + note->length()) {
                const auto x = tick * ClipEditorGlobal::pixelsPerQuarterNote /
                               static_cast<double>(AppGlobal::ticksPerQuarterNote);
                const auto y = (127 - note->keyIndex()) * pianoKeyHeight;
                snapshot->lines.append({
                    {x, y                 },
                    {x, y + pianoKeyHeight},
                    QColor(255, 100, 100), 1.5F, 30
                });
            }
        }
    }
    snapshot->rectangles.append(m_pastePreviewRects);
    snapshot->texts.append(m_pastePreviewTexts);
    return snapshot;
}

void RhiPianoRollCanvas::publishSnapshot(const EditorDirtyDomains domains) {
    if (domains == EditorDirtyDomain::Camera && m_widget->snapshot()) {
        m_widget->setSnapshot(m_widget->snapshot(), domains);
        return;
    }
    QElapsedTimer timer;
    timer.start();
    const auto snapshot = buildSnapshot();
    m_widget->setSnapshotBuildDuration(timer.nsecsElapsed());
    m_revision = snapshot->revision;
    m_widget->setSnapshot(snapshot, domains);
}

void RhiPianoRollCanvas::ensureVisibleSnapshot() {
    const auto viewport = m_widget->viewportState();
    if (!m_widget->snapshot() || viewport.startTick < m_cachedStartTick ||
        viewport.endTick > m_cachedEndTick || viewport.topValue > m_cachedTopKey ||
        viewport.bottomValue < m_cachedBottomKey) {
        refreshSnapshot(EditorDirtyDomain::Geometry);
    }
}
