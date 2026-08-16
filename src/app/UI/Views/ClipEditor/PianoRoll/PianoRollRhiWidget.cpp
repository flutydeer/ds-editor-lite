#include "PianoRollRhiWidget.h"

#include "NoteView.h"
#include "NoteEditUtils.h"
#include "PianoPaintUtils.h"
#include "PianoRollCoord.h"
#include "PitchDisplayStrategy.h"
#include "PianoRollGraphicsViewHelper.h"
#include "PronunciationView.h"
#include "Controller/ClipController.h"
#include "Global/AppGlobal.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Utils/AppColorPalette.h"
#include "UI/Utils/ITimelinePainter.h"
#include "UI/Views/ClipEditor/AnchorEditor/AnchorEditController.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/ClipEditor/CurveRenderUtils.h"
#include "UI/Views/Common/AutoPageTurnUtils.h"
#include "UI/Views/Common/EdgeAutoScroller.h"
#include "UI/Views/Common/EditorItemGeometry.h"
#include "UI/Views/Common/EditorSelectionUtils.h"
#include "UI/Views/Common/EditorResizeUtils.h"
#include "UI/Views/Common/EditorRhiGeometry.h"
#include "UI/Views/Common/EditorGlyphAtlas.h"
#include "UI/Views/Common/EditorRhiScrollBarController.h"
#include "UI/Views/Common/EditorViewportController.h"
#include "UI/Views/Common/EditorWheelController.h"
#include "Modules/Inference/EditSessionManager.h"

#include <lite/GUI/Controls/InlineTextEditOverlay.h>
#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/Utils/AppModelUtils.h>
#include <lite/Support/MathUtils.h>
#include <lite/MusicBase/TimelineSnapUtils.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QCursor>
#include <QEvent>
#include <QFontMetricsF>
#include <QHash>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLineF>
#include <QMetaObject>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSet>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <functional>
#include <climits>
#include <numbers>
#include <utility>

using namespace ClipEditorGlobal;

namespace {
    constexpr float kPitchLineWidth = 1.5f;

    using Vertex = EditorRhiSolidVertex;

    QColor blendedColor(const QColor &from, const QColor &to, const double ratio) {
        const auto t = std::clamp(ratio, 0.0, 1.0);
        return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * t,
                                from.greenF() + (to.greenF() - from.greenF()) * t,
                                from.blueF() + (to.blueF() - from.blueF()) * t,
                                from.alphaF() + (to.alphaF() - from.alphaF()) * t);
    }

    QList<PitchDisplayInterval> clippedCoverage(const QList<PitchDisplayInterval> &coverage,
                                                const double start, const double end) {
        QList<PitchDisplayInterval> result;
        for (const auto &interval : PitchDisplayStrategy::combineCoverage(coverage, {})) {
            const auto clippedStart = std::max(start, interval.startTick);
            const auto clippedEnd = std::min(end, interval.endTick);
            if (clippedEnd > clippedStart)
                result.append({clippedStart, clippedEnd});
        }
        return result;
    }

    QList<PitchDisplayInterval> subtractCoverage(const QList<PitchDisplayInterval> &base,
                                                 const QList<PitchDisplayInterval> &removed) {
        const auto normalizedBase = PitchDisplayStrategy::combineCoverage(base, {});
        const auto normalizedRemoved = PitchDisplayStrategy::combineCoverage(removed, {});
        QList<PitchDisplayInterval> result;
        for (const auto &interval : normalizedBase) {
            auto cursor = interval.startTick;
            for (const auto &cut : normalizedRemoved) {
                if (cut.endTick <= cursor)
                    continue;
                if (cut.startTick >= interval.endTick)
                    break;
                if (cut.startTick > cursor)
                    result.append({cursor, std::min(cut.startTick, interval.endTick)});
                cursor = std::max(cursor, cut.endTick);
                if (cursor >= interval.endTick)
                    break;
            }
            if (cursor < interval.endTick)
                result.append({cursor, interval.endTick});
        }
        return result;
    }

    QList<PitchDisplayInterval> intersectCoverage(const QList<PitchDisplayInterval> &first,
                                                  const QList<PitchDisplayInterval> &second) {
        const auto normalizedFirst = PitchDisplayStrategy::combineCoverage(first, {});
        const auto normalizedSecond = PitchDisplayStrategy::combineCoverage(second, {});
        QList<PitchDisplayInterval> result;
        qsizetype firstIndex = 0;
        qsizetype secondIndex = 0;
        while (firstIndex < normalizedFirst.size() && secondIndex < normalizedSecond.size()) {
            const auto &left = normalizedFirst[firstIndex];
            const auto &right = normalizedSecond[secondIndex];
            const auto start = std::max(left.startTick, right.startTick);
            const auto end = std::min(left.endTick, right.endTick);
            if (end > start)
                result.append({start, end});
            if (left.endTick < right.endTick)
                ++firstIndex;
            else
                ++secondIndex;
        }
        return result;
    }

    class TimelineLineEmitter final : public ITimelinePainter {
    public:
        using Callback = std::function<void(int tick, const QColor &color)>;

        void emitLines(const Timeline &timeline, const int quantize, const double startTick,
                       const double endTick, const double width, const QColor &barColor,
                       const QColor &beatColor, const QColor &commonColor, Callback callback) {
            setTimeline(timeline);
            setQuantize(quantize);
            m_barColor = barColor;
            m_beatColor = beatColor;
            m_commonColor = commonColor;
            m_callback = std::move(callback);

            QImage target(1, 1, QImage::Format_ARGB32_Premultiplied);
            QPainter painter(&target);
            drawTimeline(&painter, startTick, endTick, width);
        }

    private:
        void drawBar(QPainter *painter, const int tick, int) override {
            emitLine(painter, tick, m_barColor);
        }

        void drawBeat(QPainter *painter, const int tick, int, int) override {
            emitLine(painter, tick, m_beatColor);
        }

        void drawSubdivision(QPainter *painter, const int tick, const int level,
                             const int levelCount) override {
            const double ratio =
                levelCount > 1 ? static_cast<double>(level) / (levelCount - 1) : 0.0;
            emitLine(painter, tick, blendedColor(m_beatColor, m_commonColor, ratio));
        }

        void emitLine(QPainter *painter, const int tick, QColor color) const {
            color.setAlphaF(color.alphaF() * painter->opacity());
            if (m_callback)
                m_callback(tick, color);
        }

        QColor m_barColor;
        QColor m_beatColor;
        QColor m_commonColor;
        Callback m_callback;
    };
}

class PianoRollRhiWidget::Private {
public:
    enum class InlineEditField { None, Lyric, Pronunciation };
    enum class Interaction { None, Move, ResizeLeft, ResizeRight, Draw, RectSelect };

    struct PastePreviewNote {
        int localStart = 0;
        int length = 0;
        int keyIndex = 60;
        QString lyric;
        QString pronunciation;
        bool pronunciationEdited = false;
        bool overlapped = false;
    };

    explicit Private(PianoRollRhiWidget *q) : q(q), viewport(nullptr), wheel(&viewport, q) {
        viewport.setPixelsPerQuarterNote(pixelsPerQuarterNote);
        viewport.setScaleBounds(0.01, 5.0, 0.5, 8.0);
        viewport.setEnsureContentFillsViewport(true, true);
        viewport.setVerticalContent(128.0, noteHeight);
        QObject::connect(&viewport, &EditorViewportController::viewportChanged, q, [this] {
            this->q->notifyViewportChanged();
            scheduleSnapshot();
        });
        QObject::connect(&edgeAutoScroller, &EdgeAutoScroller::frame, q,
                         [this](const double dtMs) { onEdgeAutoScrollFrame(dtMs); });
    }

    ~Private() {
        clearPitchPreview();
        qDeleteAll(anchorCurves);
    }

    [[nodiscard]] double horizontalScale() const {
        return viewport.horizontalScale();
    }

    [[nodiscard]] double verticalScale() const {
        return viewport.verticalScale();
    }

    [[nodiscard]] double horizontalOffset() const {
        return viewport.horizontalOffset();
    }

    [[nodiscard]] double verticalOffset() const {
        return viewport.verticalOffset();
    }

    void initializeInlineEditor() {
        inlineEditor = new InlineTextEditOverlay(q);
        QObject::connect(inlineEditor, &InlineTextEditOverlay::textSubmitted, q,
                         [this](const QString &text) { submitInlineText(text); });
        QObject::connect(inlineEditor, &InlineTextEditOverlay::navigationRequested, q,
                         [this](const QString &text, const bool backwards) {
                             navigateInlineText(text, backwards);
                         });
        QObject::connect(inlineEditor, &InlineTextEditOverlay::editCancelled, q,
                         [this] { cancelInlineEdit(); });
    }

    void initializeScrollBars() {
        scrollBars = new EditorRhiScrollBarController(q, q);
        QObject::connect(scrollBars, &EditorRhiScrollBarController::offsetChangeRequested, q,
                         [this](const QPointF &offset) {
                             wheel.stop();
                             viewport.setOffset(offset);
                         });
    }

    void updateScrollBars() const {
        if (!scrollBars)
            return;
        const auto contentSize = clip ? QSizeF(sceneWidth(), sceneHeight()) : QSizeF(q->size());
        scrollBars->setMetrics(contentSize, viewport.offset(),
                               QSizeF(std::max(1, q->width() / 10),
                                      std::max(1.0, noteHeight * verticalScale())));
    }

    void setDataContext(SingingClip *newClip) {
        wheel.stop();
        viewport.stopAnimation();
        disarmEdgeAutoScroll();
        noteSelection.clearAnchor();
        discardNoteInteraction();
        finishNoteErase(EditSessionEndReason::Discard);
        appStatus->pianoRollNoteEditPreview = {};
        appStatus->pianoRollNoteErasePreview = {};
        finishInlineEditing();
        cancelPitchEdit();
        cancelAnchorEdit(false);
        clearSplitPreview();
        clearPastePreview();
        mergedPitchCurveCache.invalidate();
        if (clip)
            QObject::disconnect(clip, nullptr, q, nullptr);

        clip = newClip;
        viewport.setContentTickRange(0.0, clip ? clip->length() : 0.0);
        if (clip) {
            QObject::connect(clip, &SingingClip::noteChanged, q, [this] { scheduleSnapshot(); });
            QObject::connect(clip, &SingingClip::paramChanged, q,
                             [this](const ParamInfo::Name name, Param::Type) {
                                 if (name == ParamInfo::Pitch) {
                                     mergedPitchCurveCache.invalidate();
                                     if (!anchorCommitting)
                                         loadAnchorCurvesFromModel();
                                     scheduleSnapshot();
                                 }
                             });
            QObject::connect(clip, &SingingClip::propertyChanged, q, [this] {
                viewport.setContentTickRange(0.0, clip->length());
                q->notifyViewportChanged();
                scheduleSnapshot();
            });
        }
        loadAnchorCurvesFromModel();
        viewportPositionPending = clip && !q->isVisible();
        if (clip && !viewportPositionPending)
            initializeCamera();
        updateScrollBars();
        updateAutoPageTurnAvailability();
        scheduleSnapshot();
    }

    void initializeCamera() {
        if (!clip || q->width() <= 0 || q->height() <= 0)
            return;

        const auto initialViewport = !cameraInitialized;
        const auto previousOffset = viewport.offset();
        viewport.setScale(initialViewport ? 1.0 : horizontalScale(),
                          initialViewport ? 1.0 : verticalScale(), {});
        auto targetOffset = viewport.offset();
        targetOffset.setX(0.0);
        if (initialViewport)
            targetOffset.setY(std::max(0, qRound((sceneHeight() - q->height()) * 0.5)));
        if (clip->notes().count() > 0) {
            const auto *firstNote = *clip->notes().begin();
            const auto visibleTicks = q->width() / pixelsPerTick();
            targetOffset.setX(
                qRound((firstNote->localStart() - visibleTicks * 0.3) * pixelsPerTick()));
            const auto noteCenterY = PianoRollCoord::keyIndexToCenterY(
                firstNote->keyIndex(), noteHeight * verticalScale());
            targetOffset.setY(qRound(noteCenterY - q->height() * 0.5));
        }
        cameraInitialized = true;
        if (initialViewport) {
            viewport.setOffset(targetOffset);
            q->notifyViewportChanged();
            return;
        }

        viewport.setOffset({targetOffset.x(), previousOffset.y()});
        viewport.setOffset(targetOffset, true);
    }

    void resize() {
        viewport.setViewportSize(q->size());
        if (!cameraInitialized) {
            if (q->isVisible())
                initializeCamera();
        }
    }

    void show() {
        if (viewportPositionPending) {
            viewportPositionPending = false;
            initializeCamera();
            updateScrollBars();
            scheduleSnapshot();
        }
    }

    void setTrackColorIndex(const int index) {
        trackColorIndex = index;
        scheduleSnapshot();
    }

    double startTick() const {
        return (clip ? clip->start() : 0) + viewport.startTick();
    }

    double endTick() const {
        return (clip ? clip->start() : 0) + viewport.endTick();
    }

    double topKeyIndex() const {
        return 127.0 - viewport.topUnit();
    }

    double bottomKeyIndex() const {
        return 127.0 - viewport.bottomUnit();
    }

    double centerKeyIndex() const {
        return PianoRollCoord::centerYToKeyIndex(viewport.state().centerUnit, 1.0);
    }

    bool centerAt(const double tick, const double keyIndex) {
        if (!clip || !std::isfinite(tick) || !std::isfinite(keyIndex))
            return false;
        return viewport.centerAt(tick - clip->start(),
                                 PianoRollCoord::keyIndexToCenterY(keyIndex, 1.0));
    }

    bool setViewScale(const double horizontal, const double vertical) {
        if (!std::isfinite(horizontal) || !std::isfinite(vertical) || horizontal <= 0.0 ||
            vertical <= 0.0)
            return false;
        return viewport.setScale(horizontal, vertical,
                                 QPointF(q->width() * 0.5, q->height() * 0.5));
    }

    int horizontalBarValue() const {
        return qRound(horizontalOffset());
    }

    void setHorizontalBarValue(const int value) {
        viewport.setOffset({static_cast<double>(value), verticalOffset()});
    }

    void setPlaybackPosition(const double tick) {
        playbackPosition = tick;
        handleAutoPageTurn();
        if (!snapshotScheduled)
            updatePlaybackOverlay();
    }

    void updatePlaybackOverlay() {
        playbackOverlayRects.clear();
        if (clip && q->width() > 0 && q->height() > 0) {
            const auto currentX = viewport.tickToSceneX(playbackPosition - clip->start()) * dpr;
            EditorRhiGeometry::appendAntialiasedVerticalOverlay(
                playbackOverlayRects, currentX - horizontalOffset() * dpr, 0.0,
                q->height() * dpr, dpr, q->playPosIndicatorColor());
        }
        playbackOverlayRects = q->submitOverlay(std::move(playbackOverlayRects));
    }

    void setAutoPageTurn(const bool enabled) {
        autoPageTurn = enabled;
        if (enabled)
            handleAutoPageTurn();
    }

    void handleAutoPageTurn() {
        if (!autoPageTurn || !autoPageTurnAvailable || !clip || edgeAutoScroller.isRunning() ||
            appStatus->currentEditObject != AppStatus::EditObjectType::None ||
            interaction != Interaction::None || pitchEditing || anchorDragging || anchorSelecting) {
            return;
        }

        const auto viewportStart = startTick();
        const auto viewportEnd = endTick();
        const auto viewportLength = viewportEnd - viewportStart;
        if (viewportLength <= 0.0)
            return;

        if (playbackPosition > viewportEnd) {
            const auto targetStart = playbackPosition - viewportLength;
            if (targetStart > viewportEnd)
                viewport.setStartTick(playbackPosition - clip->start());
            else
                viewport.scrollBy({static_cast<double>(q->width()), 0.0});
        } else if (playbackPosition < viewportStart) {
            viewport.setStartTick(playbackPosition - clip->start());
        } else {
            return;
        }
    }

    void updateAutoPageTurnAvailability() {
        const auto available = clip && AutoPageTurnUtils::isPageDurationAvailable(
                                           appModel->timeline(), startTick(), endTick());
        if (autoPageTurnAvailable != available) {
            autoPageTurnAvailable = available;
            emit q->autoPageTurnAvailabilityChanged(available);
        }
    }

    HistoryFocusVisibility focusVisibility(const HistoryFocus &focus) const {
        if (!clip || focus.kind != HistoryFocusKind::PianoRollNotes || !focus.isValid())
            return HistoryFocusVisibility::Unavailable;
        if (focus.containerId >= 0 && focus.containerId != clip->id())
            return HistoryFocusVisibility::ContextSwitchRequired;
        QList<int> resolvedIds;
        const auto bounds = focusSceneRect(focus, &resolvedIds);
        if (!resolvedIds.isEmpty()) {
            return viewport.logicalVisibleSceneRect().intersects(bounds)
                       ? HistoryFocusVisibility::Visible
                       : HistoryFocusVisibility::ScrollRequired;
        }
        const auto tickOffset = focus.ticksAreLocal ? clip->start() : 0.0;
        const auto logical = viewport.logicalVisibleSceneRect();
        const auto visibleStartTick = clip->start() + viewport.sceneXToTick(logical.left());
        const auto visibleEndTick = clip->start() + viewport.sceneXToTick(logical.right());
        const auto tickVisible =
            focus.tickEnd + tickOffset >= visibleStartTick &&
            focus.tickStart + tickOffset <= visibleEndTick;
        const auto topKey = 127.0 - viewport.sceneYToUnit(logical.top());
        const auto bottomKey = 127.0 - viewport.sceneYToUnit(logical.bottom());
        const auto keyVisible = focus.valueEnd >= bottomKey && focus.valueStart <= topKey;
        return tickVisible && keyVisible ? HistoryFocusVisibility::Visible
                                         : HistoryFocusVisibility::ScrollRequired;
    }

    bool revealFocus(const HistoryFocus &focus, const bool animated) {
        if (focusVisibility(focus) == HistoryFocusVisibility::Unavailable || !clip)
            return false;
        if (focus.containerId >= 0 && focus.containerId != clip->id())
            return false;
        wheel.stop();
        QList<int> selected;
        const auto bounds = focusSceneRect(focus, &selected);
        syncNoteSelection(selected);
        return ensureSceneRectVisible(bounds, 24.0, 24.0, animated);
    }

    void horizontalScale(QWheelEvent *event) {
        if (!clip)
            return;
        wheel.horizontalScale(event);
    }

    void verticalScale(QWheelEvent *event) {
        if (!clip)
            return;
        wheel.verticalScale(event);
    }

    void horizontalScroll(QWheelEvent *event) {
        wheel.horizontalScroll(event);
    }

    void verticalScroll(QWheelEvent *event) {
        wheel.verticalScroll(event);
    }

    QPointF scenePositionAt(const QPointF &viewportPosition) const {
        return viewport.viewportToScene(viewportPosition);
    }

    Qt::Orientations edgeAutoScrollAxes() const {
        if (noteErasing || anchorDragging || anchorSelecting || interaction == Interaction::Move ||
            (interaction == Interaction::RectSelect && editMode != IntervalSelect)) {
            return Qt::Horizontal | Qt::Vertical;
        }
        if (interaction == Interaction::ResizeLeft || interaction == Interaction::ResizeRight ||
            interaction == Interaction::Draw ||
            (interaction == Interaction::RectSelect && editMode == IntervalSelect)) {
            return Qt::Horizontal;
        }
        return {};
    }

    void prepareEdgeAutoScroll(const QPointF &pressPosition) {
        edgeAutoScroller.prepareDrag(pressPosition);
    }

    void disarmEdgeAutoScroll() {
        edgeAutoScroller.stopDrag();
    }

    void updateEdgeAutoScrollState(const QPointF &pointerPosition) {
        const auto axes = edgeAutoScrollAxes();
        if (!axes) {
            disarmEdgeAutoScroll();
            return;
        }

        const QRectF viewportRect(QPointF(), q->size());
        edgeAutoScroller.setDragAxes(axes);
        edgeAutoScroller.updateDragState(pointerPosition, viewportRect,
                                         QApplication::startDragDistance());
    }

    void continueEdgeDragAt(const QPointF &viewportPosition,
                            const Qt::KeyboardModifiers modifiers) {
        if (editMode == EditPitchAnchor) {
            if (anchorDragging)
                updateAnchorDrag(viewportPosition);
            else if (anchorSelecting)
                updateAnchorSelection(viewportPosition);
            return;
        }
        if (noteErasing) {
            eraseNoteAt(viewportPosition);
            return;
        }
        if (interaction == Interaction::Draw) {
            updateDrawNote(viewportPosition);
        } else if (interaction == Interaction::RectSelect) {
            updateRubberBandSelection(viewportPosition);
        } else if (interaction != Interaction::None) {
            updateInteractionDelta(viewportPosition, modifiers);
            scheduleSnapshot();
        }
    }

    void onEdgeAutoScrollFrame(const double dtMs) {
        if (!edgeAutoScroller.isDragArmed() || QGuiApplication::mouseButtons() == Qt::NoButton ||
            !q->isVisible()) {
            disarmEdgeAutoScroll();
            return;
        }

        const QPointF pointerPosition(q->mapFromGlobal(QCursor::pos()));
        const QRectF viewportRect(QPointF(), q->size());
        const auto step = edgeAutoScroller.computeDragStep(pointerPosition, viewportRect, dtMs);
        if (!step.isNull())
            viewport.scrollBy(step);
        continueEdgeDragAt(EdgeAutoScroller::clampToRect(pointerPosition, viewportRect),
                           QGuiApplication::queryKeyboardModifiers());
        updateEdgeAutoScrollState(pointerPosition);
    }

    int keyAt(const QPointF &viewportPosition) const {
        const auto row =
            static_cast<int>(std::floor(viewport.sceneYToUnit(
                viewport.viewportToScene(viewportPosition).y())));
        return std::clamp(127 - row, 0, 127);
    }

    double localTickAt(const QPointF &viewportPosition) const {
        return viewport.sceneXToTick(viewport.viewportToScene(viewportPosition).x());
    }

    Note *noteAt(const QPointF &viewportPosition) const {
        if (!clip)
            return nullptr;
        const auto tick = localTickAt(viewportPosition);
        const auto key = keyAt(viewportPosition);
        for (auto iterator = clip->notes().rbegin(); iterator != clip->notes().rend(); ++iterator) {
            auto *note = *iterator;
            if (erasedNoteIds.contains(note->id()))
                continue;
            if (note->keyIndex() == key && tick >= note->localStart() &&
                tick <= note->localStart() + note->length())
                return note;
        }
        return nullptr;
    }

    QList<int> orderedNoteIds() const {
        QList<const Note *> ordered;
        if (clip) {
            ordered.reserve(clip->notes().count());
            for (const auto *note : clip->notes())
                ordered.append(note);
        }
        std::sort(ordered.begin(), ordered.end(), [](const Note *left, const Note *right) {
            if (left->localStart() != right->localStart())
                return left->localStart() < right->localStart();
            return left->id() < right->id();
        });

        QList<int> result;
        result.reserve(ordered.size());
        for (const auto *note : ordered)
            result.append(note->id());
        return result;
    }

    void syncNoteSelection(const QList<int> &selection) {
        noteSelection.synchronize(selection);
        if (appStatus->selectedNotes.get() != selection)
            clipController->selectNotes(selection, true);
    }

    bool noteEditingEnabled() const {
        return editMode == Select || editMode == IntervalSelect || editMode == DrawNote;
    }

    Interaction noteInteractionAt(const Note *note, const QPointF &viewportPosition) const {
        if (!note)
            return Interaction::None;
        const auto rect = noteViewportRect(note);
        const auto relativeX = viewportPosition.x() - rect.left();
        const auto edge = EditorResizeUtils::horizontalEdgeAt(
            relativeX, rect.width(), AppGlobal::resizeTolerance);
        if (edge == EditorResizeUtils::HorizontalEdge::Left)
            return Interaction::ResizeLeft;
        if (edge == EditorResizeUtils::HorizontalEdge::Right)
            return Interaction::ResizeRight;
        return Interaction::Move;
    }

    void updateNoteCursor(const QPointF &viewportPosition) const {
        if (!noteEditingEnabled()) {
            q->setCursor(Qt::ArrowCursor);
            return;
        }
        const auto interaction = noteInteractionAt(noteAt(viewportPosition), viewportPosition);
        const auto resizing =
            interaction == Interaction::ResizeLeft || interaction == Interaction::ResizeRight;
        q->setCursor(resizing ? Qt::SizeHorCursor : Qt::ArrowCursor);
    }

    void eraseNoteAt(const QPointF &viewportPosition) {
        auto *note = noteAt(viewportPosition);
        if (!note || erasedNoteIds.contains(note->id()))
            return;
        if (noteEraseSessionId == 0 && !editSessionManager->hasActiveTransaction()) {
            noteEraseSessionId = editSessionManager->beginTransaction(
                AppStatus::EditObjectType::Note, clip ? clip->id() : -1, {}, {}, {}, {}, true);
        }
        appStatus->currentEditObject = AppStatus::EditObjectType::Note;
        erasedNoteIds.append(note->id());
        appStatus->pianoRollNoteErasePreview = erasedNoteIds;
        editSessionManager->addNoteIds({note->id()});
        scheduleSnapshot();
    }

    void finishNoteErase(const EditSessionEndReason reason) {
        if (!noteErasing && erasedNoteIds.isEmpty() && noteEraseSessionId == 0)
            return;
        if (reason == EditSessionEndReason::Commit && !erasedNoteIds.isEmpty())
            clipController->onRemoveNotes(erasedNoteIds);
        appStatus->pianoRollNoteErasePreview = {};
        erasedNoteIds.clear();
        noteErasing = false;
        if (noteEraseSessionId != 0 && editSessionManager->hasActiveTransaction() &&
            editSessionManager->activeSession().sessionId == noteEraseSessionId) {
            editSessionManager->endTransaction(noteEraseSessionId, reason);
        }
        noteEraseSessionId = 0;
        if (!editSessionManager->hasActiveTransaction())
            appStatus->currentEditObject = AppStatus::EditObjectType::None;
        scheduleSnapshot();
    }

    void clearSplitPreview() {
        if (splitPreviewNoteId < 0)
            return;
        splitPreviewNoteId = -1;
        splitPreviewTick = 0;
        scheduleSnapshot();
    }

    void updateSplitPreview(const QPointF &viewportPosition) {
        if (!clip || editMode != SplitNote) {
            clearSplitPreview();
            return;
        }

        auto *note = noteAt(viewportPosition);
        if (!note) {
            clearSplitPreview();
            return;
        }

        const auto quantizedTickLength =
            TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
        const auto globalTick = localTickAt(viewportPosition) + clip->start();
        const auto snappedGlobalTick =
            TimelineSnapUtils::snapNearest(globalTick, quantizedTickLength, appModel->timeline());
        const auto snappedLocalTick = snappedGlobalTick - clip->start();
        const auto noteEnd = note->localStart() + note->length();
        if (snappedLocalTick <= note->localStart() || snappedLocalTick >= noteEnd) {
            clearSplitPreview();
            return;
        }

        if (splitPreviewNoteId == note->id() && splitPreviewTick == snappedLocalTick)
            return;
        splitPreviewNoteId = note->id();
        splitPreviewTick = snappedLocalTick;
        scheduleSnapshot();
    }

    void setPastePreview(const PianoRollPastePreviewData &data, const int globalTick) {
        pastePreviewNotes.clear();
        if (!clip || data.notes.isEmpty()) {
            scheduleSnapshot();
            return;
        }

        const auto localPreviewStart = globalTick - clip->start();
        pastePreviewNotes.reserve(data.notes.size());
        for (const auto &note : data.notes)
            pastePreviewNotes.append({note.relativeStart + localPreviewStart, note.length, note.key,
                                      note.lyric, note.pronunciation, note.pronunciationEdited,
                                      note.overlapped});
        scheduleSnapshot();
    }

    void clearPastePreview() {
        if (pastePreviewNotes.isEmpty())
            return;
        pastePreviewNotes.clear();
        scheduleSnapshot();
    }

    QRectF noteViewportRect(const Note *note) const {
        if (!note)
            return {};
        return noteSceneRect(note).translated(-viewport.offset());
    }

    Note *pronunciationAt(const QPointF &viewportPosition) const {
        if (!clip)
            return nullptr;
        for (auto iterator = clip->notes().rbegin(); iterator != clip->notes().rend(); ++iterator) {
            auto *note = *iterator;
            const auto noteRect = noteViewportRect(note);
            const QRectF pronunciationRect(noteRect.left(), noteRect.bottom(), noteRect.width(),
                                           20.0);
            if (pronunciationRect.contains(viewportPosition) &&
                !note->pronunciation().result().isEmpty()) {
                return note;
            }
        }
        return nullptr;
    }

    struct NoteContextTarget {
        Note *note = nullptr;
        bool pronunciation = false;
    };

    NoteContextTarget noteContextTargetAt(const QPointF &viewportPosition) const {
        if (auto *note = pronunciationAt(viewportPosition))
            return {note, true};
        return {noteAt(viewportPosition), false};
    }

    QRect inlineAnchorRect(const QRectF &source) const {
        const auto viewportRect = q->rect();
        QRect anchorRect = source.toAlignedRect();
        const int width = std::min(viewportRect.width(), std::max(40, anchorRect.width()));
        const int height = std::min(viewportRect.height(), std::max(20, anchorRect.height()));
        anchorRect.setSize({width, height});
        anchorRect.moveLeft(std::clamp(anchorRect.left(), 0, viewportRect.width() - width));
        anchorRect.moveTop(std::clamp(anchorRect.top(), 0, viewportRect.height() - height));
        return anchorRect;
    }

    void startLyricEdit(Note *note) {
        if (!note || !inlineEditor)
            return;
        if (inlineEditField == InlineEditField::Lyric && inlineEditingNoteId == note->id() &&
            inlineEditor->isEditing()) {
            return;
        }
        finishInlineEditing();
        inlineEditField = InlineEditField::Lyric;
        inlineEditingNoteId = note->id();
        QFont font = q->font();
        font.setPixelSize(q->noteFontPixelSize());
        const QVariantMap properties = {
            {QStringLiteral("editRole"),          QStringLiteral("Lyric")},
            {QStringLiteral("navigationEnabled"), true                   },
        };
        inlineEditor->showAt(inlineAnchorRect(noteViewportRect(note)), note->lyric(), font,
                             properties);
        scheduleSnapshot();
    }

    void startPronunciationEdit(Note *note) {
        if (!note || !inlineEditor)
            return;
        if (inlineEditField == InlineEditField::Pronunciation &&
            inlineEditingNoteId == note->id() && inlineEditor->isEditing()) {
            return;
        }
        finishInlineEditing();
        inlineEditField = InlineEditField::Pronunciation;
        inlineEditingNoteId = note->id();
        const auto noteRect = noteViewportRect(note);
        const QRectF pronunciationRect(noteRect.left(), noteRect.bottom(), noteRect.width(), 20.0);
        QFont font = q->font();
        font.setPixelSize(q->noteFontPixelSize());
        const auto pronunciation = note->pronunciation();
        const auto text = pronunciation.isEdited() ? pronunciation.edited : pronunciation.original;
        const QVariantMap properties = {
            {QStringLiteral("editRole"), QStringLiteral("Pronunciation")},
        };
        inlineEditor->showAt(inlineAnchorRect(pronunciationRect), text, font, properties);
        scheduleSnapshot();
    }

    void finishInlineEditing() {
        if (inlineEditor && inlineEditor->isEditing())
            inlineEditor->submit();
    }

    void cancelInlineEdit() {
        inlineEditField = InlineEditField::None;
        inlineEditingNoteId = -1;
        scheduleSnapshot();
    }

    void submitInlineText(const QString &text) {
        const auto field = inlineEditField;
        const auto noteId = inlineEditingNoteId;
        cancelInlineEdit();
        if (!clip)
            return;
        const auto *note = clip->findNoteById(noteId);
        if (!note)
            return;
        if (field == InlineEditField::Lyric) {
            auto lyric = text.trimmed();
            if (lyric.isEmpty())
                lyric = appOptions->general()->defaultLyricForLanguage(note->language());
            if (lyric != note->lyric())
                clipController->onNoteLyricEdited(noteId, lyric);
        } else if (field == InlineEditField::Pronunciation) {
            const auto pronunciation = note->pronunciation();
            const auto displayText =
                pronunciation.isEdited() ? pronunciation.edited : pronunciation.original;
            const auto editedText = text.trimmed();
            if (editedText != displayText && editedText != pronunciation.edited)
                clipController->onNotePronunciationEdited(noteId, editedText);
        }
    }

    void navigateInlineText(const QString &text, const bool backwards) {
        const auto currentId = inlineEditingNoteId;
        submitInlineText(text);
        if (!clip)
            return;
        QList<Note *> ordered;
        for (auto *note : clip->notes())
            ordered.append(note);
        std::sort(ordered.begin(), ordered.end(), [](const Note *left, const Note *right) {
            if (left->localStart() != right->localStart())
                return left->localStart() < right->localStart();
            if (left->keyIndex() != right->keyIndex())
                return left->keyIndex() > right->keyIndex();
            return left->id() < right->id();
        });
        const auto current =
            std::find_if(ordered.begin(), ordered.end(),
                         [currentId](const Note *note) { return note->id() == currentId; });
        if (current == ordered.end())
            return;
        const auto index = static_cast<qsizetype>(std::distance(ordered.begin(), current));
        const auto target = backwards ? index - 1 : index + 1;
        if (target < 0 || target >= ordered.size())
            return;
        clipController->selectNotes({ordered.at(target)->id()}, true);
        startLyricEdit(ordered.at(target));
    }

    QPoint pitchPointAt(const QPointF &viewportPosition) const {
        const auto tick = std::max(
            0, MathUtils::round(static_cast<int>(localTickAt(viewportPosition)), DrawCurve().step));
        const auto sceneY = viewport.viewportToScene(viewportPosition).y();
        const auto value = qRound((127.5 - viewport.sceneYToUnit(sceneY)) * 100.0);
        return {tick, std::clamp(value, 0, 12700)};
    }

    DrawCurve *pitchCurveAt(const int tick) const {
        for (auto *curve : pitchPreviewCurves) {
            if (curve->localStart() <= tick && curve->localEndTick() > tick)
                return curve;
        }
        return nullptr;
    }

    void clearPitchPreview() {
        qDeleteAll(pitchPreviewCurves);
        pitchPreviewCurves.clear();
        pitchEditingCurve = nullptr;
        mergedPitchCurveCache.invalidate();
    }

    void finishPitchEdit(const EditSessionEndReason reason) {
        if (pitchEditSessionId != 0 && editSessionManager->hasActiveTransaction() &&
            editSessionManager->activeSession().sessionId == pitchEditSessionId) {
            editSessionManager->endTransaction(pitchEditSessionId, reason);
        }
        pitchEditSessionId = 0;
        if (!editSessionManager->hasActiveTransaction())
            appStatus->currentEditObject = AppStatus::EditObjectType::None;
    }

    void cancelPitchEdit(const bool update = true) {
        if (!pitchEditing)
            return;
        clearPitchPreview();
        pitchEditing = false;
        pitchMouseMoved = false;
        pitchNewCurveCreated = false;
        pitchEditType = PitchEditType::None;
        finishPitchEdit(EditSessionEndReason::Discard);
        if (update)
            scheduleSnapshot();
    }

    void beginPitchEdit(const QPointF &viewportPosition) {
        const auto *pitch = clip->params.getParamByName(ParamInfo::Pitch);
        if (!pitch)
            return;

        clearPitchPreview();
        for (const auto *curve : pitch->curves(Param::Edited)) {
            if (curve->type() == Curve::Draw)
                MathUtils::binaryInsert(pitchPreviewCurves,
                                        new DrawCurve(*static_cast<const DrawCurve *>(curve)));
        }

        pitchMouseDownPos = pitchPointAt(viewportPosition);
        pitchPreviousPos = pitchMouseDownPos;
        pitchMouseMoved = false;
        pitchNewCurveCreated = false;
        pitchEditingCurve = nullptr;
        if (editMode == ErasePitch) {
            pitchEditType = PitchEditType::Erase;
        } else if ((pitchEditingCurve = pitchCurveAt(pitchMouseDownPos.x()))) {
            pitchEditType = PitchEditType::DrawOnCurve;
        } else {
            pitchEditType = PitchEditType::DrawOnInterval;
        }

        pitchEditSessionId = editSessionManager->beginTransaction(
            AppStatus::EditObjectType::Param, clip->id(), {}, {}, {}, {ParamInfo::Pitch});
        appStatus->currentEditObject = AppStatus::EditObjectType::Param;
        pitchEditing = true;
        scheduleSnapshot();
    }

    static void drawPitchLine(const QPoint &p1, const QPoint &p2, DrawCurve &curve) {
        if (p1.x() == p2.x())
            return;

        const auto startPoint = p1.x() < p2.x() ? p1 : p2;
        const auto endPoint = p1.x() < p2.x() ? p2 : p1;
        DrawCurve line(-1);
        line.setLocalStart(startPoint.x());
        const auto pointCount = (endPoint.x() - startPoint.x()) / curve.step;
        for (int i = 0; i < pointCount; ++i) {
            const auto tick = startPoint.x() + i * curve.step;
            line.appendValue(qRound(MathUtils::linearValueAt(startPoint, endPoint, tick)));
        }
        curve.mergeWithOtherPriority(line);
    }

    void updatePitchEdit(const QPointF &viewportPosition) {
        if (!pitchEditing || pitchEditType == PitchEditType::None)
            return;

        const auto current = pitchPointAt(viewportPosition);
        if (current == pitchPreviousPos)
            return;
        if (current.x() == pitchPreviousPos.x())
            return;
        pitchMouseMoved = true;
        const auto startTick = std::min(pitchPreviousPos.x(), current.x());
        const auto endTick = std::max(pitchPreviousPos.x(), current.x());
        const auto overlapped = AppModelUtils::curvesIn(pitchPreviewCurves, startTick, endTick);

        if (pitchEditType == PitchEditType::Erase) {
            for (auto *curve : overlapped) {
                if (curve->localStart() >= startTick && curve->localEndTick() <= endTick) {
                    pitchPreviewCurves.removeOne(curve);
                    delete curve;
                } else if (curve->localStart() < startTick && curve->localEndTick() > endTick) {
                    auto *rightCurve = new DrawCurve;
                    rightCurve->setLocalStart(endTick);
                    rightCurve->setValues(curve->mid(endTick));
                    curve->eraseTailFrom(startTick);
                    MathUtils::binaryInsert(pitchPreviewCurves, rightCurve);
                } else {
                    curve->erase(startTick, endTick);
                }
            }
        } else {
            if (!pitchNewCurveCreated && pitchEditType == PitchEditType::DrawOnInterval) {
                pitchEditingCurve = new DrawCurve;
                pitchEditingCurve->setLocalStart(pitchMouseDownPos.x());
                pitchEditingCurve->appendValue(pitchMouseDownPos.y());
                MathUtils::binaryInsert(pitchPreviewCurves, pitchEditingCurve);
                pitchNewCurveCreated = true;
            }

            drawPitchLine(pitchPreviousPos, current, *pitchEditingCurve);
            for (auto *curve : overlapped) {
                if (curve == pitchEditingCurve)
                    continue;
                pitchEditingCurve->mergeWithCurrentPriority(*curve);
                pitchPreviewCurves.removeOne(curve);
                delete curve;
            }
        }

        mergedPitchCurveCache.invalidate();
        pitchPreviousPos = current;
        scheduleSnapshot();
    }

    void commitPitchEdit(const QPointF &viewportPosition) {
        if (!pitchEditing)
            return;
        updatePitchEdit(viewportPosition);
        if (pitchMouseMoved)
            PianoRollGraphicsViewHelper::editPitch(pitchPreviewCurves);
        clearPitchPreview();
        pitchEditing = false;
        pitchMouseMoved = false;
        pitchNewCurveCreated = false;
        pitchEditType = PitchEditType::None;
        finishPitchEdit(EditSessionEndReason::Commit);
        scheduleSnapshot();
    }

    QPoint anchorPointAt(const QPointF &viewportPosition) const {
        const auto tick = std::max(0, static_cast<int>(localTickAt(viewportPosition)));
        const auto sceneY = viewport.viewportToScene(viewportPosition).y();
        const auto value = qRound((127.5 - viewport.sceneYToUnit(sceneY)) * 100.0);
        return {tick, std::clamp(value, 0, 12700)};
    }

    QPointF anchorNodeViewportPosition(const AnchorNode *node) const {
        return {viewport.tickToSceneX(node->pos()) - horizontalOffset(),
                viewport.unitToSceneY((12700 - node->value() + 50) / 100.0) -
                    verticalOffset()};
    }

    void finishAnchorEditSession(const EditSessionEndReason reason) {
        if (anchorEditSessionId != 0 && editSessionManager->hasActiveTransaction() &&
            editSessionManager->activeSession().sessionId == anchorEditSessionId) {
            editSessionManager->endTransaction(anchorEditSessionId, reason);
        }
        anchorEditSessionId = 0;
        if (!editSessionManager->hasActiveTransaction())
            appStatus->currentEditObject = AppStatus::EditObjectType::None;
    }

    void beginAnchorEditSession() {
        if (!clip || anchorEditSessionId != 0)
            return;
        anchorEditSessionId = editSessionManager->beginTransaction(
            AppStatus::EditObjectType::Param, clip->id(), {}, {}, {}, {ParamInfo::Pitch});
        appStatus->currentEditObject = AppStatus::EditObjectType::Param;
    }

    void resetAnchorInteraction() {
        anchorEditing = false;
        anchorDragging = false;
        anchorSelecting = false;
        anchorCursorInView = true;
        currentAnchorCurve = nullptr;
        selectedAnchorNodes.clear();
        hoveredAnchorNode = nullptr;
        anchorDragInfos.clear();
        anchorSelectionRect = {};
        anchorPreviewCurve = nullptr;
        anchorMergeCandidateCurve = nullptr;
        anchorMergeEndpointNode = nullptr;
        showAnchorPreview = false;
        showAnchorMergePreview = false;
    }

    void loadAnchorCurvesFromModel() {
        finishAnchorEditSession(EditSessionEndReason::Discard);
        resetAnchorInteraction();
        qDeleteAll(anchorCurves);
        anchorCurves.clear();
        if (!clip)
            return;
        const auto *pitch = clip->params.getParamByName(ParamInfo::Pitch);
        if (!pitch)
            return;
        for (const auto *curve : pitch->curves(Param::Edited)) {
            if (curve->type() == Curve::Anchor)
                anchorCurves.append(new AnchorCurve(*static_cast<const AnchorCurve *>(curve)));
        }
    }

    void cancelAnchorEdit(const bool reload = true) {
        finishAnchorEditSession(EditSessionEndReason::Discard);
        resetAnchorInteraction();
        if (reload)
            loadAnchorCurvesFromModel();
        scheduleSnapshot();
    }

    AnchorNode *anchorNodeAt(const QPointF &viewportPosition) const {
        constexpr double hitRadius = 6.0;
        for (const auto *curve : anchorCurves) {
            for (auto *node : curve->nodes().toList()) {
                const auto delta = viewportPosition - anchorNodeViewportPosition(node);
                if (QPointF::dotProduct(delta, delta) <= hitRadius * hitRadius)
                    return node;
            }
        }
        return nullptr;
    }

    AnchorCurve *anchorCurveAt(const int tick, const AnchorCurve *exclude = nullptr) const {
        for (auto *curve : anchorCurves) {
            if (curve == exclude)
                continue;
            const auto nodes = curve->nodes().toList();
            if (!nodes.isEmpty() && tick >= nodes.first()->pos() && tick <= nodes.last()->pos())
                return curve;
        }
        return nullptr;
    }

    AnchorCurve *findAnchorOwner(const AnchorNode *node) const {
        for (auto *curve : anchorCurves) {
            if (curve->nodes().toList().contains(const_cast<AnchorNode *>(node)))
                return curve;
        }
        return nullptr;
    }

    static AnchorNode *findAnchorNodeAtTick(AnchorCurve *curve, const int tick,
                                            const AnchorNode *exclude = nullptr) {
        if (!curve)
            return nullptr;
        for (auto *node : curve->nodes().toList()) {
            if (node != exclude && node->pos() == tick)
                return node;
        }
        return nullptr;
    }

    std::pair<int, int> anchorReachableBounds(const AnchorCurve *curve) const {
        if (!curve)
            return {INT_MIN, INT_MAX};
        const auto curveNodes = curve->nodes().toList();
        if (curveNodes.isEmpty())
            return {INT_MIN, INT_MAX};

        int minimum = INT_MIN;
        int maximum = INT_MAX;
        for (const auto *other : anchorCurves) {
            if (other == curve)
                continue;
            const auto nodes = other->nodes().toList();
            if (nodes.isEmpty())
                continue;
            if (nodes.last()->pos() < curveNodes.first()->pos())
                minimum = std::max(minimum, nodes.last()->pos() + 1);
            if (nodes.first()->pos() > curveNodes.last()->pos())
                maximum = std::min(maximum, nodes.first()->pos() - 1);
        }
        return {minimum, maximum};
    }

    void clearAnchorSelection() {
        selectedAnchorNodes.clear();
    }

    void selectAnchorNode(AnchorNode *node) {
        selectedAnchorNodes = {node};
        currentAnchorCurve = findAnchorOwner(node);
    }

    void enterAnchorEditing(AnchorCurve *curve, AnchorNode *node = nullptr) {
        anchorEditing = true;
        currentAnchorCurve = curve;
        if (node)
            selectAnchorNode(node);
    }

    void exitAnchorEditing() {
        anchorEditing = false;
        currentAnchorCurve = nullptr;
        showAnchorPreview = false;
        showAnchorMergePreview = false;
        clearAnchorSelection();
    }

    void cleanupEmptyAnchorCurve(AnchorCurve *curve) {
        if (!curve || !curve->nodes().toList().isEmpty())
            return;
        anchorCurves.removeOne(curve);
        if (currentAnchorCurve == curve)
            currentAnchorCurve = nullptr;
        delete curve;
        if (!currentAnchorCurve)
            exitAnchorEditing();
    }

    void removeOverlappingAnchorNodes(AnchorCurve *curve, AnchorNode *keep) {
        if (!curve || !keep)
            return;
        QList<AnchorNode *> remove;
        for (auto *node : curve->nodes().toList()) {
            if (node != keep && node->pos() == keep->pos())
                remove.append(node);
        }
        for (auto *node : remove) {
            curve->removeNode(node);
            selectedAnchorNodes.removeOne(node);
            if (hoveredAnchorNode == node)
                hoveredAnchorNode = nullptr;
            delete node;
        }
    }

    void commitAnchorEdit() {
        if (!clip)
            return;
        beginAnchorEditSession();

        QList<Curve *> combined;
        const auto *pitch = clip->params.getParamByName(ParamInfo::Pitch);
        if (pitch) {
            for (const auto *curve : pitch->curves(Param::Edited)) {
                if (curve->type() == Curve::Draw)
                    combined.append(new DrawCurve(*static_cast<const DrawCurve *>(curve)));
            }
        }
        for (const auto *curve : anchorCurves)
            combined.append(new AnchorCurve(*curve));

        anchorCommitting = true;
        clipController->onParamEdited(ParamInfo::Pitch, combined);
        anchorCommitting = false;
        finishAnchorEditSession(EditSessionEndReason::Commit);
        scheduleSnapshot();
    }

    void createAnchorAt(const QPointF &viewportPosition) {
        const auto point = anchorPointAt(viewportPosition);
        AnchorCurve *curve = nullptr;
        if (anchorEditing && currentAnchorCurve) {
            if (auto *other = anchorCurveAt(point.x(), currentAnchorCurve)) {
                curve = other;
            } else {
                const auto [minimum, maximum] = anchorReachableBounds(currentAnchorCurve);
                if (point.x() >= minimum && point.x() <= maximum)
                    curve = currentAnchorCurve;
            }
        } else {
            curve = anchorCurveAt(point.x());
        }
        if (!curve) {
            curve = new AnchorCurve;
            anchorCurves.append(curve);
        }

        if (auto *existing = findAnchorNodeAtTick(curve, point.x())) {
            enterAnchorEditing(curve, existing);
            return;
        }

        const auto nodes = curve->nodes().toList();
        auto *oldLast = nodes.isEmpty() ? nullptr : nodes.last();
        auto *node = new AnchorNode(point.x(), point.y());
        if (!oldLast || point.x() > oldLast->pos()) {
            node->setInterpMode(AnchorNode::None);
            if (oldLast) {
                auto mode = oldLast->interpMode();
                if (mode == AnchorNode::None) {
                    const auto index = nodes.indexOf(oldLast);
                    mode = index > 0 ? nodes.at(index - 1)->interpMode() : AnchorNode::Hermite;
                }
                oldLast->setInterpMode(mode);
            }
        } else {
            auto mode = AnchorNode::Hermite;
            for (int i = nodes.size() - 1; i >= 0; --i) {
                if (nodes.at(i)->pos() < point.x()) {
                    mode = nodes.at(i)->interpMode();
                    break;
                }
            }
            node->setInterpMode(mode);
        }
        curve->insertNode(node);
        enterAnchorEditing(curve, node);
        commitAnchorEdit();
    }

    void deleteSelectedAnchorNodes() {
        if (selectedAnchorNodes.isEmpty())
            return;
        QHash<AnchorCurve *, QList<AnchorNode *>> byCurve;
        for (auto *node : selectedAnchorNodes) {
            if (auto *curve = findAnchorOwner(node))
                byCurve[curve].append(node);
        }
        clearAnchorSelection();
        for (auto it = byCurve.begin(); it != byCurve.end(); ++it) {
            auto *curve = it.key();
            for (auto *node : it.value()) {
                curve->removeNode(node);
                delete node;
            }
            const auto remaining = curve->nodes().toList();
            if (remaining.isEmpty()) {
                anchorCurves.removeOne(curve);
                if (currentAnchorCurve == curve)
                    currentAnchorCurve = nullptr;
                delete curve;
            } else {
                remaining.last()->setInterpMode(AnchorNode::None);
            }
        }
        if (!currentAnchorCurve)
            exitAnchorEditing();
        commitAnchorEdit();
    }

    void updateAnchorMergeCandidate(const QPointF &viewportPosition) {
        anchorMergeCandidateCurve = nullptr;
        anchorMergeEndpointNode = nullptr;
        showAnchorMergePreview = false;
        if (!anchorEditing || !currentAnchorCurve)
            return;

        const auto currentNodes = currentAnchorCurve->nodes().toList();
        if (currentNodes.isEmpty())
            return;
        for (auto *curve : anchorCurves) {
            if (curve == currentAnchorCurve)
                continue;
            const auto nodes = curve->nodes().toList();
            if (nodes.isEmpty())
                continue;
            AnchorNode *candidate = nullptr;
            if (nodes.last()->pos() < currentNodes.first()->pos())
                candidate = nodes.last();
            else if (nodes.first()->pos() > currentNodes.last()->pos())
                candidate = nodes.first();
            if (!candidate)
                continue;
            const auto delta = viewportPosition - anchorNodeViewportPosition(candidate);
            if (QPointF::dotProduct(delta, delta) <= 36.0) {
                anchorMergeCandidateCurve = curve;
                anchorMergeEndpointNode = candidate;
                showAnchorMergePreview = true;
                return;
            }
        }
    }

    void updateAnchorPreview(const QPointF &viewportPosition) {
        anchorPreviewPosition = viewportPosition;
        showAnchorPreview = anchorEditing && currentAnchorCurve && !hoveredAnchorNode &&
                            !showAnchorMergePreview && anchorCursorInView;
        anchorPreviewCurve = nullptr;
        if (!showAnchorPreview)
            return;
        const auto tick = anchorPointAt(viewportPosition).x();
        if (auto *other = anchorCurveAt(tick, currentAnchorCurve)) {
            anchorPreviewCurve = other;
        } else {
            const auto [minimum, maximum] = anchorReachableBounds(currentAnchorCurve);
            if (tick >= minimum && tick <= maximum)
                anchorPreviewCurve = currentAnchorCurve;
        }
    }

    void mergeAnchorCurves(AnchorCurve *target) {
        if (!currentAnchorCurve || !target || target == currentAnchorCurve)
            return;
        const auto nodes = target->nodes().toList();
        for (auto *node : nodes) {
            target->removeNode(node);
            if (findAnchorNodeAtTick(currentAnchorCurve, node->pos()))
                delete node;
            else
                currentAnchorCurve->insertNode(node);
        }
        anchorCurves.removeOne(target);
        delete target;
        anchorMergeCandidateCurve = nullptr;
        anchorMergeEndpointNode = nullptr;
        showAnchorMergePreview = false;
        commitAnchorEdit();
    }

    void updateAnchorSelection(const QPointF &viewportPosition) {
        anchorSelectionRect.setBottomRight(scenePositionAt(viewportPosition));
        const auto rect = anchorSelectionRect.normalized();
        clearAnchorSelection();
        for (auto *curve : anchorCurves) {
            for (auto *node : curve->nodes().toList()) {
                if (rect.contains(anchorNodeScenePosition(node)))
                    selectedAnchorNodes.append(node);
            }
        }
        scheduleSnapshot();
    }

    void updateAnchorDrag(const QPointF &viewportPosition) {
        if (anchorDragInfos.isEmpty()) {
            for (auto *node : selectedAnchorNodes) {
                anchorDragInfos.append(
                    {node, findAnchorOwner(node), nullptr, node->pos(), node->value()});
            }
        }
        const auto current = anchorPointAt(viewportPosition);
        const auto deltaTick = current.x() - anchorDragStartPoint.x();
        const auto deltaValue = current.y() - anchorDragStartPoint.y();
        for (auto &info : anchorDragInfos) {
            if (!info.sourceCurve)
                continue;
            info.sourceCurve->removeNode(info.node);
            info.node->setPos(std::max(0, info.startTick + deltaTick));
            info.node->setValue(std::clamp(info.startValue + deltaValue, 0, 12700));
            info.sourceCurve->insertNode(info.node);
            info.targetCurve = anchorCurveAt(info.node->pos(), info.sourceCurve);
        }
        scheduleSnapshot();
    }

    void mousePressAnchor(QMouseEvent *event) {
        beginAnchorEditSession();
        auto *node = anchorNodeAt(event->position());
        if (anchorEditing) {
            if (node) {
                if (showAnchorMergePreview && node == anchorMergeEndpointNode) {
                    mergeAnchorCurves(anchorMergeCandidateCurve);
                } else {
                    if (!selectedAnchorNodes.contains(node))
                        selectAnchorNode(node);
                    anchorDragStartPoint = anchorPointAt(event->position());
                    anchorDragPressViewportPos = event->position();
                    anchorDragging = false;
                    anchorDragInfos.clear();
                }
            } else {
                createAnchorAt(event->position());
                anchorDragStartPoint = anchorPointAt(event->position());
                anchorDragPressViewportPos = event->position();
                anchorDragging = false;
                anchorDragInfos.clear();
            }
        } else if (node) {
            selectAnchorNode(node);
            enterAnchorEditing(currentAnchorCurve, node);
            anchorDragStartPoint = anchorPointAt(event->position());
            anchorDragPressViewportPos = event->position();
            anchorDragging = false;
            anchorDragInfos.clear();
        } else {
            anchorSelectionRect = QRectF(scenePositionAt(event->position()), QSizeF());
            anchorSelecting = true;
        }
        scheduleSnapshot();
    }

    void mouseMoveAnchor(QMouseEvent *event) {
        if (event->buttons().testFlag(Qt::LeftButton)) {
            if (anchorEditing && !selectedAnchorNodes.isEmpty()) {
                if (!anchorDragging &&
                    QLineF(anchorDragPressViewportPos, event->position()).length() > 3.0) {
                    anchorDragging = true;
                }
                if (anchorDragging)
                    updateAnchorDrag(event->position());
            } else if (anchorSelecting) {
                updateAnchorSelection(event->position());
            }
            return;
        }

        auto *hovered = anchorNodeAt(event->position());
        if (hoveredAnchorNode != hovered)
            hoveredAnchorNode = hovered;
        updateAnchorMergeCandidate(event->position());
        updateAnchorPreview(event->position());
        scheduleSnapshot();
    }

    void mouseReleaseAnchor(QMouseEvent *event) {
        if (anchorDragging) {
            anchorDragging = false;
            QSet<AnchorCurve *> sourcesToCleanup;
            for (auto &info : anchorDragInfos) {
                if (info.sourceCurve && info.targetCurve && info.targetCurve != info.sourceCurve) {
                    info.sourceCurve->removeNode(info.node);
                    info.targetCurve->insertNode(info.node);
                    sourcesToCleanup.insert(info.sourceCurve);
                }
            }
            for (auto &info : anchorDragInfos) {
                auto *finalCurve = info.targetCurve ? info.targetCurve : info.sourceCurve;
                removeOverlappingAnchorNodes(finalCurve, info.node);
            }
            for (auto *curve : sourcesToCleanup)
                cleanupEmptyAnchorCurve(curve);
            if (!selectedAnchorNodes.isEmpty())
                currentAnchorCurve = findAnchorOwner(selectedAnchorNodes.first());
            anchorDragInfos.clear();
            updateAnchorPreview(event->position());
            commitAnchorEdit();
            return;
        }
        if (anchorSelecting) {
            anchorSelecting = false;
            if (!selectedAnchorNodes.isEmpty()) {
                QSet<AnchorCurve *> involved;
                for (auto *node : selectedAnchorNodes)
                    involved.insert(findAnchorOwner(node));
                if (involved.size() == 1)
                    enterAnchorEditing(*involved.begin());
                else
                    anchorEditing = true;
            }
            anchorSelectionRect = {};
        }
        finishAnchorEditSession(EditSessionEndReason::Discard);
        scheduleSnapshot();
    }

    bool keyPressAnchor(QKeyEvent *event) {
        if (event->key() == Qt::Key_Escape && anchorEditing) {
            if (anchorDragging)
                loadAnchorCurvesFromModel();
            else
                exitAnchorEditing();
            finishAnchorEditSession(EditSessionEndReason::Discard);
            scheduleSnapshot();
            return true;
        }
        return false;
    }

    void beginNoteEditSession(const QList<int> &noteIds, const bool wholeClipScope = false) {
        if (!clip || noteEditSessionId != 0)
            return;
        noteEditSessionId = editSessionManager->beginTransaction(
            AppStatus::EditObjectType::Note, clip->id(), {}, noteIds, {}, {}, wholeClipScope);
        appStatus->currentEditObject = AppStatus::EditObjectType::Note;
    }

    void finishNoteEditSession(const EditSessionEndReason reason) {
        if (noteEditSessionId != 0 && editSessionManager->hasActiveTransaction() &&
            editSessionManager->activeSession().sessionId == noteEditSessionId) {
            editSessionManager->endTransaction(noteEditSessionId, reason);
        }
        noteEditSessionId = 0;
        if (!editSessionManager->hasActiveTransaction())
            appStatus->currentEditObject = AppStatus::EditObjectType::None;
    }

    void publishNoteEditPreview() const {
        QVector<AppStatus::NoteEditPreview> preview;
        if (!clip) {
            appStatus->pianoRollNoteEditPreview = preview;
            return;
        }

        if (interaction == Interaction::Draw) {
            preview.append({-1, drawStart, drawEnd - drawStart, drawKey});
        } else if (interaction == Interaction::Move) {
            for (const auto id : appStatus->selectedNotes.get()) {
                if (const auto *note = clip->findNoteById(id)) {
                    preview.append({id, note->localStart() + interactionDeltaTick, note->length(),
                                    note->keyIndex() + interactionDeltaKey});
                }
            }
        } else if (interaction == Interaction::ResizeLeft ||
                   interaction == Interaction::ResizeRight) {
            if (const auto *note = clip->findNoteById(interactionNoteId)) {
                const auto start = interaction == Interaction::ResizeLeft
                                       ? note->localStart() + interactionDeltaTick
                                       : note->localStart();
                preview.append({note->id(), start,
                                note->length() + (interaction == Interaction::ResizeLeft
                                                      ? -interactionDeltaTick
                                                      : interactionDeltaTick),
                                note->keyIndex()});
            }
        }
        appStatus->pianoRollNoteEditPreview = preview;
    }

    void resetNoteInteraction() {
        interaction = Interaction::None;
        interactionNoteId = -1;
        interactionDeltaTick = 0;
        interactionDeltaKey = 0;
        interactionMinimumLength = 1;
        interactionMoved = false;
        drawStart = 0;
        drawEnd = 0;
    }

    void discardNoteInteraction() {
        noteSelection.cancelPress();
        if (interaction == Interaction::None && noteEditSessionId == 0) {
            return;
        }
        appStatus->pianoRollNoteEditPreview = {};
        resetNoteInteraction();
        finishNoteEditSession(EditSessionEndReason::Discard);
        scheduleSnapshot();
    }

    void beginDrawNote(const QPointF &viewportPosition) {
        discardNoteInteraction();
        syncNoteSelection({});
        noteSelection.clearAnchor();
        interaction = Interaction::Draw;
        const auto step = TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
        const auto snappedStart =
            NoteEditUtils::snapLocalDown(localTickAt(viewportPosition) + clip->start(),
                                         clip->start(), step, appModel->timeline());
        drawStart = std::max(0, snappedStart);
        drawEnd = drawStart + step;
        drawKey = keyAt(viewportPosition);
        beginNoteEditSession({}, true);
        publishNoteEditPreview();
        scheduleSnapshot();
    }

    EditorSelectionUtils::PressResult updateNoteSelection(const Note *note,
                                                          const Qt::KeyboardModifiers modifiers) {
        const auto noteId = note ? note->id() : -1;
        const auto result = noteSelection.press(appStatus->selectedNotes.get(), orderedNoteIds(),
                                                noteId, modifiers);
        syncNoteSelection(result.selection);
        return result;
    }

    void updateContextNoteSelection(const Note *note) {
        noteSelection.cancelPress();
        const auto noteId = note ? note->id() : -1;
        const auto result =
            noteSelection.applyPress(appStatus->selectedNotes.get(), orderedNoteIds(), noteId,
                                     EditorSelectionUtils::SelectionMode::Plain);
        syncNoteSelection(result.selection);
    }

    void mousePress(QMouseEvent *event) {
        if (!clip)
            return;
        viewport.stopAnimation();
        if (event->button() != Qt::LeftButton) {
            if (!EditorViewGlobal::isPitchEditMode(editMode))
                updateContextNoteSelection(noteContextTargetAt(event->position()).note);
            return;
        }
        if (noteErasing)
            finishNoteErase(EditSessionEndReason::Discard);
        discardNoteInteraction();
        if (editMode == EditPitchAnchor) {
            mousePressAnchor(event);
            return;
        }
        if (editMode == DrawPitch || editMode == ErasePitch) {
            beginPitchEdit(event->position());
            return;
        }
        auto *note = noteAt(event->position());
        if (editMode == EraseNote) {
            noteErasing = true;
            eraseNoteAt(event->position());
            return;
        }
        if (editMode == SplitNote) {
            updateSplitPreview(event->position());
            if (note && splitPreviewNoteId == note->id())
                PianoRollGraphicsViewHelper::splitNote(note->id(),
                                                       splitPreviewTick + clip->start());
            clearSplitPreview();
            return;
        }
        if (!noteEditingEnabled())
            return;
        if (editMode == DrawNote) {
            syncNoteSelection({});
            noteSelection.clearAnchor();
            if (const auto *pronunciationNote = pronunciationAt(event->position())) {
                syncNoteSelection({pronunciationNote->id()});
                return;
            }
            if (!note) {
                beginDrawNote(event->position());
                return;
            }
        }
        if (!note) {
            noteSelection.clearAnchor();
            noteSelection.cancelPress();
            interaction = Interaction::RectSelect;
            rubberBandStart = scenePositionAt(event->position());
            rubberBandEnd = rubberBandStart;
            rubberBandBaseSelection = event->modifiers().testFlag(Qt::ControlModifier)
                                          ? appStatus->selectedNotes.get()
                                          : QList<int>();
            syncNoteSelection(rubberBandBaseSelection);
            scheduleSnapshot();
            return;
        }

        const auto selectionResult = updateNoteSelection(note, event->modifiers());
        if (!selectionResult.targetSelected)
            return;

        interactionNoteId = note->id();
        interactionStart = note->localStart();
        interactionLength = note->length();
        interactionKey = note->keyIndex();
        mouseDownTick = localTickAt(event->position());
        mouseDownKey = keyAt(event->position());
        interaction = noteInteractionAt(note, event->position());
        interactionMoved = false;
        if (interaction == Interaction::ResizeLeft || interaction == Interaction::ResizeRight)
            q->setCursor(Qt::SizeHorCursor);
        scheduleSnapshot();
    }

    void mouseMove(QMouseEvent *event) {
        const auto key = keyAt(event->position());
        if (key != hoveredKey) {
            hoveredKey = key;
            emit q->keyHovered(key);
        }
        if (event->buttons() == Qt::NoButton)
            updateNoteCursor(event->position());
        if (editMode == EditPitchAnchor) {
            anchorCursorInView = true;
            mouseMoveAnchor(event);
            return;
        }
        if (editMode == SplitNote) {
            updateSplitPreview(event->position());
            return;
        }
        if (pitchEditing) {
            updatePitchEdit(event->position());
            return;
        }
        if (noteErasing) {
            eraseNoteAt(event->position());
            return;
        }
        continueEdgeDragAt(event->position(), event->modifiers());
    }

    void updateDrawNote(const QPointF &viewportPosition) {
        const auto step = TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
        const auto snappedEnd =
            NoteEditUtils::snapLocalDown(localTickAt(viewportPosition) + clip->start(),
                                         clip->start(), step, appModel->timeline());
        drawEnd = drawStart + NoteEditUtils::lengthForSnappedEnd(drawStart, snappedEnd, step);
        publishNoteEditPreview();
        scheduleSnapshot();
    }

    void updateRubberBandSelection(const QPointF &viewportPosition) {
        rubberBandEnd = scenePositionAt(viewportPosition);
        auto selected = rubberBandBaseSelection;
        auto selection = QRectF(rubberBandStart, rubberBandEnd).normalized();
        if (editMode == IntervalSelect) {
            selection.setTop(0.0);
            selection.setBottom(sceneHeight());
        }
        for (const auto *note : clip->notes()) {
            const QRectF rect(viewport.tickToSceneX(note->localStart()),
                              viewport.unitToSceneY(127 - note->keyIndex()),
                              note->length() * pixelsPerTick(),
                              noteHeight * verticalScale());
            if (selection.intersects(rect) && !selected.contains(note->id()))
                selected.append(note->id());
        }
        syncNoteSelection(selected);
        scheduleSnapshot();
    }

    void mouseRelease(QMouseEvent *event) {
        if (!clip || event->button() != Qt::LeftButton)
            return;
        if (editMode == EditPitchAnchor) {
            mouseReleaseAnchor(event);
            return;
        }
        if (pitchEditing) {
            commitPitchEdit(event->position());
            return;
        }
        if (noteErasing) {
            finishNoteErase(EditSessionEndReason::Commit);
            return;
        }
        if (interaction == Interaction::Draw) {
            PianoRollGraphicsViewHelper::drawNote(drawStart, drawEnd - drawStart, drawKey);
            appStatus->pianoRollNoteEditPreview = {};
            finishNoteEditSession(EditSessionEndReason::Commit);
        } else if (interaction != Interaction::None && interaction != Interaction::RectSelect &&
                   interactionNoteId >= 0) {
            if (interactionMoved && interaction == Interaction::Move) {
                if (interactionDeltaTick != 0 || interactionDeltaKey != 0)
                    clipController->onMoveNotes(appStatus->selectedNotes.get(),
                                                interactionDeltaTick, interactionDeltaKey);
            } else if (interactionMoved && interaction == Interaction::ResizeLeft &&
                       interactionDeltaTick != 0) {
                clipController->onResizeNotesLeft({interactionNoteId}, interactionDeltaTick,
                                                  interactionMinimumLength);
            } else if (interactionMoved && interaction == Interaction::ResizeRight &&
                       interactionDeltaTick != 0) {
                clipController->onResizeNotesRight({interactionNoteId}, interactionDeltaTick,
                                                   interactionMinimumLength);
            }
            appStatus->pianoRollNoteEditPreview = {};
            finishNoteEditSession(interactionMoved ? EditSessionEndReason::Commit
                                                   : EditSessionEndReason::Discard);
        }
        syncNoteSelection(noteSelection.release(appStatus->selectedNotes.get(), interactionMoved));
        resetNoteInteraction();
        updateNoteCursor(event->position());
        scheduleSnapshot();
    }

    void updateInteractionDelta(const QPointF &position, const Qt::KeyboardModifiers modifiers) {
        if (interaction == Interaction::None || interaction == Interaction::Draw)
            return;
        if (!interactionMoved) {
            interactionMoved = true;
            const auto ids = interaction == Interaction::Move ? appStatus->selectedNotes.get()
                                                              : QList<int>{interactionNoteId};
            beginNoteEditSession(ids);
        }
        const auto step = TimelineSnapUtils::quantizeStep(appStatus->pianoRollQuantize,
                                                          modifiers == Qt::AltModifier);
        interactionMinimumLength = step;
        const auto rawDelta = localTickAt(position) - mouseDownTick;
        interactionDeltaKey = keyAt(position) - mouseDownKey;

        if (interaction == Interaction::Move) {
            interactionDeltaTick = NoteEditUtils::moveDelta(rawDelta, step);
            int minimumKey = 127;
            int maximumKey = 0;
            int minimumStart = INT_MAX;
            bool found = false;
            for (const auto id : appStatus->selectedNotes.get()) {
                if (const auto *selectedNote = clip->findNoteById(id)) {
                    minimumKey = std::min(minimumKey, selectedNote->keyIndex());
                    maximumKey = std::max(maximumKey, selectedNote->keyIndex());
                    minimumStart = std::min(minimumStart, selectedNote->localStart());
                    found = true;
                }
            }
            if (found) {
                interactionDeltaKey =
                    std::clamp(interactionDeltaKey, -minimumKey, 127 - maximumKey);
                interactionDeltaTick =
                    NoteResizeUtils::clampLeftMoveDelta(interactionDeltaTick, minimumStart);
            }
        } else if (interaction == Interaction::ResizeLeft) {
            const auto snappedTick = NoteEditUtils::snapLocalDown(
                localTickAt(position) + clip->start(), clip->start(), step, appModel->timeline());
            interactionDeltaTick = NoteEditUtils::leftResizeDelta(
                interactionStart, interactionLength, snappedTick, step);
        } else if (interaction == Interaction::ResizeRight) {
            const auto snappedTick = NoteEditUtils::snapLocalNearest(
                localTickAt(position) + clip->start(), clip->start(), step, appModel->timeline());
            interactionDeltaTick = NoteEditUtils::rightResizeDelta(
                interactionStart, interactionLength, snappedTick, step);
        }
        publishNoteEditPreview();
    }

    void scheduleSnapshot() {
        if (snapshotScheduled)
            return;
        snapshotScheduled = true;
        QTimer::singleShot(0, q, [this] {
            snapshotScheduled = false;
            rebuildSnapshot();
        });
    }

    void rebuildSnapshot() {
        auto frame = q->acquireFrame();
        vertices = std::move(frame.solidVertices);
        vertices.clear();
        drawList = std::move(frame.drawList);
        drawList.clear();
        dpr = q->devicePixelRatioF();
        glyphAtlas.beginFrame();

        if (clip && q->width() > 0 && q->height() > 0) {
            const auto localStart = viewport.startTick();
            const auto localEnd = viewport.endTick();
            const auto sceneTop = verticalOffset();
            const auto sceneBottom = verticalOffset() + q->height();

            appendBackground(localStart, localEnd, sceneTop, sceneBottom);
            appendTimeline(localStart, localEnd, sceneTop, sceneBottom);
            appendNotes(localStart, localEnd);
            appendPastePreview(localStart, localEnd);
            appendPitch(localStart, localEnd);
            appendAnchors(localStart, localEnd);
            appendClipMask(localStart, localEnd, sceneTop, sceneBottom);
            appendLastPlaybackIndicator(sceneTop, sceneBottom);
            appendRubberBand();
            appendSplitPreview();
        }
        frame.clearColor = q->whiteKeyColor();
        frame.physicalCameraOffset = physicalCameraOffset();
        drawList.finish(vertices.size());
        frame.solidVertices = std::move(vertices);
        frame.drawList = std::move(drawList);
        glyphAtlas.populateTextureBatches(frame.textureBatches);
        q->submitFrame(std::move(frame));
        updatePlaybackOverlay();
    }

private:
    QRectF noteSceneRect(const Note *note) const {
        if (!note)
            return {};
        return {viewport.tickToSceneX(note->localStart()),
                viewport.unitToSceneY(127 - note->keyIndex()),
                note->length() * pixelsPerTick(), noteHeight * verticalScale()};
    }

    QRectF focusSceneRect(const HistoryFocus &focus, QList<int> *resolvedIds = nullptr) const {
        QRectF bounds;
        for (const auto id : focus.objectIds) {
            if (const auto *note = clip->findNoteById(id)) {
                if (resolvedIds)
                    resolvedIds->append(id);
                const auto rect = noteSceneRect(note);
                bounds = bounds.isNull() ? rect : bounds.united(rect);
            }
        }
        if (!bounds.isNull())
            return bounds;

        const auto localStart =
            focus.ticksAreLocal ? focus.tickStart : focus.tickStart - clip->start();
        const auto localEnd = focus.ticksAreLocal ? focus.tickEnd : focus.tickEnd - clip->start();
        const auto keyHeight = noteHeight * verticalScale();
        const auto top = (127.0 - focus.valueEnd) * keyHeight;
        const auto bottom = (127.0 - focus.valueStart) * keyHeight + keyHeight;
        return {localStart * pixelsPerTick(), top,
                std::max(1.0, (localEnd - localStart) * pixelsPerTick()),
                std::max(keyHeight, bottom - top)};
    }

    bool ensureSceneRectVisible(const QRectF &rect, const double xMargin, const double yMargin,
                                const bool animated) {
        const auto bounds = rect.normalized();
        if (!bounds.isValid() || !std::isfinite(bounds.left()) ||
            !std::isfinite(bounds.top()) || !std::isfinite(bounds.right()) ||
            !std::isfinite(bounds.bottom())) {
            return false;
        }
        return viewport.ensureVisible(bounds, xMargin, yMargin, animated);
    }

    QPointF physicalCameraOffset() const {
        return viewport.offset() * dpr;
    }

    double pixelsPerTick() const {
        return pixelsPerQuarterNote * horizontalScale() / AppGlobal::ticksPerQuarterNote;
    }

    double sceneWidth() const {
        return clip ? viewport.tickToSceneX(clip->length()) : q->width();
    }

    double sceneHeight() const {
        return viewport.unitToSceneY(128.0);
    }

    void appendVertex(const double x, const double y, const QColor &color,
                      const float coverage = 1.0f) {
        const auto alpha = static_cast<float>(color.alphaF());
        vertices.append({static_cast<float>(x), static_cast<float>(y),
                         static_cast<float>(color.redF()) * alpha,
                         static_cast<float>(color.greenF()) * alpha,
                         static_cast<float>(color.blueF()) * alpha, alpha, coverage});
    }

    void appendPhysicalRect(const double left, const double top, const double right,
                            const double bottom, const QColor &color) {
        if (right <= left || bottom <= top || color.alpha() == 0)
            return;
        appendVertex(left, top, color);
        appendVertex(right, top, color);
        appendVertex(right, bottom, color);
        appendVertex(left, top, color);
        appendVertex(right, bottom, color);
        appendVertex(left, bottom, color);
    }

    void appendLogicalRect(const QRectF &rect, const QColor &color) {
        appendPhysicalRect(rect.left() * dpr, rect.top() * dpr, rect.right() * dpr,
                           rect.bottom() * dpr, color);
    }

    void appendPixelAlignedVerticalLine(const double x, const double top, const double bottom,
                                        const QColor &color) {
        EditorRhiGeometry::appendAntialiasedVerticalLine(vertices, x * dpr, top * dpr, bottom * dpr,
                                                         dpr, color, horizontalOffset() * dpr);
    }

    void appendPixelAlignedHorizontalLine(const double y, const double left, const double right,
                                          const QColor &color) {
        EditorRhiGeometry::appendAntialiasedHorizontalLine(vertices, y * dpr, left * dpr,
                                                           right * dpr, dpr, color,
                                                           verticalOffset() * dpr);
    }

    void appendLine(const QPointF &from, const QPointF &to, const double logicalWidth,
                    const QColor &color) {
        const QPointF p1 = from * dpr;
        const QPointF p2 = to * dpr;
        const auto dx = p2.x() - p1.x();
        const auto dy = p2.y() - p1.y();
        const auto length = std::hypot(dx, dy);
        if (length <= 0.0001)
            return;
        const auto halfWidth = logicalWidth * dpr * 0.5;
        const QPointF normal(-dy / length * halfWidth, dx / length * halfWidth);
        const auto a = p1 + normal;
        const auto b = p2 + normal;
        const auto c = p2 - normal;
        const auto d = p1 - normal;
        appendVertex(a.x(), a.y(), color);
        appendVertex(b.x(), b.y(), color);
        appendVertex(c.x(), c.y(), color);
        appendVertex(a.x(), a.y(), color);
        appendVertex(c.x(), c.y(), color);
        appendVertex(d.x(), d.y(), color);
    }

    void appendBackground(const double localStart, const double localEnd, const double sceneTop,
                          const double sceneBottom) {
        const auto left = localStart * pixelsPerTick();
        const auto right = localEnd * pixelsPerTick();
        const auto white = q->whiteKeyColor();
        const auto black = q->blackKeyColor();
        const auto octave = q->octaveDividerColor();
        appendLogicalRect(QRectF(left, sceneTop, right - left, sceneBottom - sceneTop), white);

        const auto firstKey =
            std::min(127, static_cast<int>(std::ceil(127.0 - viewport.sceneYToUnit(sceneTop))));
        const auto lastKey =
            std::max(0,
                     static_cast<int>(std::floor(127.0 - viewport.sceneYToUnit(sceneBottom))));
        for (int key = firstKey; key >= lastKey; --key) {
            const auto y = viewport.unitToSceneY(127 - key);
            appendLogicalRect(QRectF(left, y, right - left, noteHeight * verticalScale()),
                              PianoPaintUtils::isWhiteKey(key) ? white : black);
            if ((key + 1) % 12 == 0)
                appendPixelAlignedHorizontalLine(y, left, right, octave);
        }
    }

    void appendTimeline(const double localStart, const double localEnd, const double sceneTop,
                        const double sceneBottom) {
        const auto globalStart = localStart + clip->start();
        const auto globalEnd = localEnd + clip->start();
        const auto width = std::max(1.0, (localEnd - localStart) * pixelsPerTick());
        const auto bar = q->barLineColor();
        const auto beat = q->beatLineColor();
        const auto common = q->commonLineColor();
        timelineEmitter.emitLines(
            appModel->timeline(), appStatus->pianoRollQuantize, globalStart, globalEnd, width, bar,
            beat, common, [this, sceneTop, sceneBottom](const int tick, const QColor &color) {
                const auto x = (tick - clip->start()) * pixelsPerTick();
                appendPixelAlignedVerticalLine(x, sceneTop, sceneBottom, color);
            });
    }

    void appendFullNoteShape(const QRectF &rect, const QColor &fill, const QColor &border) {
        if (rect.isEmpty())
            return;
        const auto rawPhysical = QRectF(rect.topLeft() * dpr, rect.size() * dpr);
        const auto physical = EditorItemGeometry::notePaintRect(rawPhysical, dpr);
        const auto radius = EditorItemGeometry::adaptiveCornerRadius(
            physical, EditorItemGeometry::noteCornerRadius * dpr);
        EditorRhiGeometry::appendRoundedRect(vertices, physical, radius, fill);
        EditorRhiGeometry::appendRoundedRectStroke(
            vertices, physical, radius, EditorItemGeometry::noteBorderWidth * dpr, border, 0.5);
    }

    void appendCompactNoteShape(const QRectF &rect, const QColor &fill) {
        const auto width = std::max(2.0, rect.width() - EditorItemGeometry::noteBorderWidth);
        const auto height = std::max(2.0, rect.height() - EditorItemGeometry::noteBorderWidth);
        appendLogicalRect(QRectF(rect.left() + EditorItemGeometry::noteBorderWidth * 0.5,
                                 rect.top() + EditorItemGeometry::noteBorderWidth * 0.5, width,
                                 height),
                          fill);
    }

    void appendNoteText(const QRectF &rect, const QString &lyric, const QString &pronunciation,
                        const QColor &foreground, const QColor &pronunciationColor,
                        const bool editingLyric, const bool editingPronunciation) {
        if (editingLyric)
            return;

        QFont font;
        font.setPixelSize(std::max(1, q->noteFontPixelSize()));
        const QFontMetricsF metrics(font);
        const auto padded = EditorItemGeometry::notePaintRect(rect);
        const auto textRect = padded.adjusted(2.0, 0.0, -2.0, 0.0);
        const auto textWidth =
            std::max(metrics.horizontalAdvance(lyric), metrics.horizontalAdvance(pronunciation));
        if (textWidth >= textRect.width() || metrics.height() >= textRect.height())
            return;

        const auto physicalTextRect = QRectF(textRect.topLeft() * dpr, textRect.size() * dpr);
        const auto textTop =
            physicalTextRect.top() + (physicalTextRect.height() - metrics.height() * dpr) * 0.5;
        const auto lyricSpan = glyphAtlas.appendText(
            lyric, font, QPointF(physicalTextRect.left(), textTop), foreground, physicalTextRect,
            dpr, physicalCameraOffset(), q->physicalWindowOffset());
        drawList.appendTexture(lyricSpan, vertices.size());

        if (pronunciation.isEmpty() || editingPronunciation)
            return;
        QFont pronunciationFont = q->font();
        const QRectF pronunciationRect(
            (rect.left() + EditorItemGeometry::noteBorderWidth + 2.0) * dpr, rect.bottom() * dpr,
            (rect.width() - EditorItemGeometry::noteBorderWidth * 2.0 - 4.0) * dpr, 20.0 * dpr);
        const auto pronunciationSpan = glyphAtlas.appendText(
            pronunciation, pronunciationFont, pronunciationRect.topLeft(), pronunciationColor,
            pronunciationRect, dpr, physicalCameraOffset(), q->physicalWindowOffset());
        drawList.appendTexture(pronunciationSpan, vertices.size());
    }

    void appendNotes(const double localStart, const double localEnd) {
        const auto *palette = AppColorPalette::instance();
        const auto normalFill = palette->noteBackground(trackColorIndex);
        const auto normalBorder = palette->noteBorder(trackColorIndex);
        const auto selectedFill = palette->noteBackgroundSelected(trackColorIndex);
        const auto selectedBorder = q->noteSelectedBorderColor();
        const auto overlappedFill = palette->noteBackgroundOverlapped(trackColorIndex);
        const auto overlappedBorder = palette->noteBorderOverlapped(trackColorIndex);
        const auto editingFill = palette->noteBackgroundEditingPitch(trackColorIndex);
        const auto editingBorder = palette->noteBorderEditingPitch(trackColorIndex);
        const auto normalForeground = palette->noteForeground(trackColorIndex);
        const auto overlappedForeground = palette->noteForegroundOverlapped(trackColorIndex);
        const auto editingForeground = palette->noteForegroundEditingPitch(trackColorIndex);
        const auto editingPitch = EditorViewGlobal::isPitchEditMode(editMode);
        const auto selectedNotes = appStatus->selectedNotes.get();
        for (const auto *note : clip->notes()) {
            if (erasedNoteIds.contains(note->id()))
                continue;
            auto noteStart = note->localStart();
            auto noteLength = note->length();
            auto noteKey = note->keyIndex();
            const auto selected = selectedNotes.contains(note->id());
            if (selected && interaction == Interaction::Move) {
                noteStart += interactionDeltaTick;
                noteKey += interactionDeltaKey;
            } else if (note->id() == interactionNoteId && interaction == Interaction::ResizeLeft) {
                noteStart += interactionDeltaTick;
                noteLength -= interactionDeltaTick;
            } else if (note->id() == interactionNoteId && interaction == Interaction::ResizeRight) {
                noteLength += interactionDeltaTick;
            }
            const auto noteEnd = noteStart + noteLength;
            if (noteEnd < localStart)
                continue;
            if (noteStart > localEnd)
                continue;
            const auto rect = QRectF(viewport.tickToSceneX(noteStart),
                                     viewport.unitToSceneY(127 - noteKey),
                                     noteLength * pixelsPerTick(), noteHeight * verticalScale());
            const auto overlapped = note->overlapped();
            const auto fill = selected       ? selectedFill
                              : overlapped   ? overlappedFill
                              : editingPitch ? editingFill
                                             : normalFill;
            const auto border = selected       ? selectedBorder
                                : overlapped   ? overlappedBorder
                                : editingPitch ? editingBorder
                                               : normalBorder;
            const auto foreground = selected       ? normalForeground
                                    : overlapped   ? overlappedForeground
                                    : editingPitch ? editingForeground
                                                   : normalForeground;
            if (horizontalScale() < 0.3) {
                appendCompactNoteShape(rect, selected       ? selectedFill
                                             : overlapped   ? overlappedBorder
                                             : editingPitch ? editingBorder
                                                            : normalFill);
                continue;
            }
            appendFullNoteShape(rect, fill, border);

            const auto pronunciation = note->pronunciation();
            const auto editingLyric =
                inlineEditField == InlineEditField::Lyric && inlineEditingNoteId == note->id();
            const auto editingPronunciation = inlineEditField == InlineEditField::Pronunciation &&
                                              inlineEditingNoteId == note->id();
            appendNoteText(rect, note->lyric(), pronunciation.result(), foreground,
                           pronunciation.isEdited() ? palette->phonemeEdited(trackColorIndex)
                                                    : q->pronunciationTextColor(),
                           editingLyric, editingPronunciation);
        }
        if (interaction == Interaction::Draw && drawEnd > drawStart) {
            const QRectF rect(viewport.tickToSceneX(drawStart),
                              viewport.unitToSceneY(127 - drawKey),
                              (drawEnd - drawStart) * pixelsPerTick(),
                              noteHeight * verticalScale());
            if (horizontalScale() < 0.3) {
                appendCompactNoteShape(rect, selectedFill);
            } else {
                appendFullNoteShape(rect, selectedFill, selectedBorder);
                appendNoteText(
                    rect, PianoRollGraphicsViewHelper::defaultLyricForNewNote(clip), {},
                    normalForeground, q->pronunciationTextColor(), false, false);
            }
        }
    }

    void appendPastePreview(const double localStart, const double localEnd) {
        if (pastePreviewNotes.isEmpty())
            return;
        const auto *palette = AppColorPalette::instance();
        auto fill = palette->noteBackground(trackColorIndex);
        auto border = palette->noteBorder(trackColorIndex);
        auto foreground = palette->noteForeground(trackColorIndex);
        auto overlappedFill = palette->noteBackgroundOverlapped(trackColorIndex);
        auto overlappedBorder = palette->noteBorderOverlapped(trackColorIndex);
        auto overlappedForeground = palette->noteForegroundOverlapped(trackColorIndex);
        auto pronunciationEdited = palette->phonemeEdited(trackColorIndex);
        auto pronunciationNormal = q->pronunciationTextColor();
        constexpr double opacity = 0.35;
        const auto applyOpacity = [opacity](QColor &color) {
            color.setAlphaF(color.alphaF() * opacity);
        };
        applyOpacity(fill);
        applyOpacity(border);
        applyOpacity(foreground);
        applyOpacity(overlappedFill);
        applyOpacity(overlappedBorder);
        applyOpacity(overlappedForeground);
        applyOpacity(pronunciationEdited);
        applyOpacity(pronunciationNormal);

        for (const auto &note : pastePreviewNotes) {
            const auto noteEnd = note.localStart + note.length;
            if (noteEnd < localStart || note.localStart > localEnd)
                continue;
            const QRectF rect(viewport.tickToSceneX(note.localStart),
                              viewport.unitToSceneY(127 - note.keyIndex),
                              note.length * pixelsPerTick(), noteHeight * verticalScale());
            const auto noteFill = note.overlapped ? overlappedFill : fill;
            const auto noteBorder = note.overlapped ? overlappedBorder : border;
            const auto noteForeground = note.overlapped ? overlappedForeground : foreground;
            if (horizontalScale() < 0.3) {
                appendCompactNoteShape(rect, note.overlapped ? noteBorder : noteFill);
                continue;
            }
            appendFullNoteShape(rect, noteFill, noteBorder);
            appendNoteText(rect, note.lyric, note.pronunciation, noteForeground,
                           note.pronunciationEdited ? pronunciationEdited : pronunciationNormal,
                           false, false);
        }
    }

    AnchorEditor::AnchorOverlayState anchorOverlayState() const {
        AnchorEditor::AnchorOverlayState state;
        state.anchorVisible = !anchorCurves.isEmpty();
        state.anchorEditActive = editMode == EditPitchAnchor;
        state.editing = anchorEditing;
        state.currentCurve = currentAnchorCurve;
        state.selectedNodes = selectedAnchorNodes;
        state.hoveredNode = hoveredAnchorNode;
        state.previewScenePos = scenePositionAt(anchorPreviewPosition);
        state.previewTick = anchorPointAt(anchorPreviewPosition).x();
        state.showPreview = showAnchorPreview;
        state.previewCurve = anchorPreviewCurve;
        state.dragStartScenePos = scenePositionAt(anchorDragPressViewportPos);
        state.dragging = anchorDragging;
        state.dragNodeInfos = anchorDragInfos;
        state.selectionSceneRect = anchorSelectionRect;
        state.selecting = anchorSelecting;
        state.visibleCurves = anchorCurves;
        state.cursorInView = anchorCursorInView;
        state.mergeCandidateCurve = anchorMergeCandidateCurve;
        state.mergeEndpointNode = anchorMergeEndpointNode;
        state.showMergePreview = showAnchorMergePreview;
        return state;
    }

    void appendPitch(const double localStart, const double localEnd) {
        const auto *pitch = clip->params.getParamByName(ParamInfo::Pitch);
        if (!pitch)
            return;
        const auto original = AppModelUtils::getDrawCurves(pitch->curves(Param::Original));
        const auto edited = pitchEditing
                                ? pitchPreviewCurves
                                : AppModelUtils::getDrawCurves(pitch->curves(Param::Edited));
        const auto anchorCoverage = PitchDisplayStrategy::anchorCoverage(anchorOverlayState());
        const auto mode = PitchDisplayStrategy::displayModeForEditMode(editMode);
        const auto layers = PitchDisplayStrategy::displayLayers(mode, edited, anchorCoverage);

        const QList<DrawCurve *> *merged = nullptr;
        if (std::any_of(layers.cbegin(), layers.cend(), [](const PitchDisplayLayer &layer) {
                return layer.curveSource == PitchDisplayCurveSource::Merged;
            })) {
            merged = &mergedPitchCurveCache.mergedCurves(original, edited);
        }
        for (const auto &layer : layers) {
            const QList<DrawCurve *> *curves = nullptr;
            switch (layer.curveSource) {
                case PitchDisplayCurveSource::Original:
                    curves = &original;
                    break;
                case PitchDisplayCurveSource::Edited:
                    curves = &edited;
                    break;
                case PitchDisplayCurveSource::Merged:
                    curves = merged;
                    break;
            }
            auto color = layer.colorRole == PitchDisplayColorRole::Original
                             ? q->paramOriginalCurveColor()
                             : q->paramEditedCurveColor();
            color.setAlpha(std::min(color.alpha(), layer.maximumAlpha));
            appendPitchLayer(*curves, localStart, localEnd, color, layer.hiddenCoverage,
                             layer.dashedCoverage);
        }
    }

    QVector<QPointF> pitchCurvePoints(const DrawCurve &curve, const double localStart,
                                      const double localEnd) const {
        const auto &values = curve.values();
        const auto start = curve.localStart();
        const auto indices =
            CurveRenderUtils::sampleCurve(curve, localStart, localEnd, pixelsPerTick(), dpr)
                .pointIndices;
        if (indices.isEmpty())
            return {};

        QVector<QPointF> points;
        points.reserve(indices.size());
        for (const auto index : indices) {
            const auto tick = start + index * curve.step;
            const auto x = tick * pixelsPerTick();
            const auto value = MathUtils::clip(values.at(index), 0, 12700);
            const auto y = viewport.unitToSceneY((12700 - value + 50) / 100.0);
            points.append(QPointF(x, y) * dpr);
        }
        return points;
    }

    void appendStrokeCoverage(const QVector<EditorRhiSolidVertex> &stroke,
                              const QList<PitchDisplayInterval> &coverage) {
        for (const auto &interval : coverage) {
            const auto left = interval.startTick * pixelsPerTick() * dpr;
            const auto right = interval.endTick * pixelsPerTick() * dpr;
            const QRectF clipRect(left, (verticalOffset() - 4.0) * dpr, right - left,
                                  (q->height() + 8.0) * dpr);
            EditorRhiGeometry::appendClippedTriangles(vertices, stroke, clipRect);
        }
    }

    void appendPitchLayer(const QList<DrawCurve *> &curves, const double localStart,
                          const double localEnd, const QColor &color,
                          const QList<PitchDisplayInterval> &hiddenCoverage,
                          const QList<PitchDisplayInterval> &dashedCoverage) {
        if (curves.isEmpty())
            return;
        const QList<PitchDisplayInterval> viewport{
            {localStart, localEnd}
        };
        const auto hidden = clippedCoverage(hiddenCoverage, localStart, localEnd);
        const auto visible = subtractCoverage(viewport, hidden);
        const auto dashed =
            intersectCoverage(visible, clippedCoverage(dashedCoverage, localStart, localEnd));
        const auto solid = subtractCoverage(visible, dashed);
        for (const auto *curve : curves) {
            if (!curve || curve->localEndTick() < localStart)
                continue;
            if (curve->localStart() > localEnd)
                break;
            const auto points = pitchCurvePoints(*curve, localStart, localEnd);
            if (points.size() < 2)
                continue;
            if (!solid.isEmpty()) {
                QVector<EditorRhiSolidVertex> stroke;
                EditorRhiGeometry::appendAntialiasedStroke(stroke, points, kPitchLineWidth * dpr,
                                                           color, 1.0, 3.0, Qt::FlatCap,
                                                           Qt::RoundJoin);
                appendStrokeCoverage(stroke, solid);
            }
            if (!dashed.isEmpty()) {
                QVector<EditorRhiSolidVertex> stroke;
                EditorRhiGeometry::appendAntialiasedDashedStroke(
                    stroke, points, kPitchLineWidth * dpr, color, 4.0 * kPitchLineWidth * dpr,
                    3.0 * kPitchLineWidth * dpr, 1.0, 3.0, Qt::FlatCap, Qt::RoundJoin);
                appendStrokeCoverage(stroke, dashed);
            }
        }
    }

    QPointF anchorNodeScenePosition(const AnchorNode *node) const {
        return {node->pos() * pixelsPerTick(),
                viewport.unitToSceneY((12700 - node->value() + 50) / 100.0)};
    }

    void appendAnchorStroke(const QList<AnchorNode *> &nodes, const double localStart,
                            const double localEnd, const QColor &color, const bool dashed = false) {
        if (nodes.size() < 2)
            return;

        QVector<QPointF> points;
        for (int i = 0; i + 1 < nodes.size(); ++i) {
            auto *first = nodes.at(i);
            auto *second = nodes.at(i + 1);
            if (second->pos() < localStart || first->pos() > localEnd)
                continue;

            auto interpolator =
                AnchorCurve::createInterpolator(first, second, i > 0 ? nodes.at(i - 1) : nullptr,
                                                i + 2 < nodes.size() ? nodes.at(i + 2) : nullptr);
            const auto startTick = std::max<double>(first->pos(), localStart);
            const auto endTick = std::min<double>(second->pos(), localEnd);
            if (endTick < startTick)
                continue;

            auto sceneX = startTick * pixelsPerTick();
            const auto endSceneX = endTick * pixelsPerTick();
            double step = 2.0;
            double previousY = 0.0;
            bool firstPoint = points.isEmpty();
            while (sceneX <= endSceneX) {
                const auto tick = sceneX / pixelsPerTick();
                const auto value = std::clamp(interpolator.evaluate(tick), 0.0, 12700.0);
                const auto sceneY = viewport.unitToSceneY((12700.0 - value + 50.0) / 100.0);
                const auto point = QPointF(sceneX, sceneY) * dpr;
                if (points.isEmpty() || QLineF(points.constLast(), point).length() > 0.01)
                    points.append(point);
                if (!firstPoint) {
                    const auto deltaY = std::abs(sceneY - previousY) * dpr;
                    step = deltaY > 4.0 ? 0.5 : deltaY > 2.0 ? 1.0 : 2.0;
                }
                firstPoint = false;
                previousY = sceneY;
                sceneX += step / dpr;
            }

            const auto value = std::clamp(interpolator.evaluate(endTick), 0.0, 12700.0);
            const auto endpoint =
                QPointF(endSceneX, viewport.unitToSceneY((12700.0 - value + 50.0) / 100.0)) *
                dpr;
            if (points.isEmpty() || QLineF(points.constLast(), endpoint).length() > 0.01)
                points.append(endpoint);
        }

        if (points.size() < 2)
            return;
        QVector<EditorRhiSolidVertex> stroke;
        if (!dashed) {
            EditorRhiGeometry::appendAntialiasedStroke(stroke, points, kPitchLineWidth * dpr, color,
                                                       1.0, 3.0, Qt::SquareCap, Qt::BevelJoin);
        } else {
            EditorRhiGeometry::appendAntialiasedDashedStroke(
                stroke, points, kPitchLineWidth * dpr, color, 4.0 * kPitchLineWidth * dpr,
                2.0 * kPitchLineWidth * dpr, 1.0, 3.0, Qt::SquareCap, Qt::BevelJoin);
        }
        vertices.reserve(vertices.size() + stroke.size());
        for (const auto &vertex : stroke)
            vertices.append(
                {vertex.x, vertex.y, vertex.r, vertex.g, vertex.b, vertex.a, vertex.coverage});
    }

    void appendAnchorNode(const AnchorNode *node, const QColor &color, const bool emphasized) {
        const auto center = anchorNodeScenePosition(node) * dpr;
        const auto radius = 2.0 * dpr;
        EditorRhiGeometry::appendRoundedRect(
            vertices, QRectF(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0),
            radius, color);
        if (!emphasized)
            return;

        QVector<QPointF> ring;
        constexpr int segments = 24;
        const auto ringRadius = 6.0 * dpr;
        ring.reserve(segments + 1);
        for (int i = 0; i <= segments; ++i) {
            const auto angle = i * 2.0 * std::numbers::pi_v<double> / segments;
            ring.append(center + QPointF(std::cos(angle), std::sin(angle)) * ringRadius);
        }
        EditorRhiGeometry::appendAntialiasedStroke(vertices, ring, 1.5 * dpr, color, 1.0, 3.0);
    }

    void appendAnchorCurve(AnchorCurve *curve, const double localStart, const double localEnd,
                           const QColor &curveColor, const QColor &nodeColor, const bool active,
                           const AnchorEditor::AnchorOverlayState &state) {
        if (!curve)
            return;
        const auto nodes = PitchDisplayStrategy::anchorCurveNodes(curve, state);
        appendAnchorStroke(nodes, localStart, localEnd, curveColor);
        for (auto *node : nodes) {
            const auto selected = active && selectedAnchorNodes.contains(node);
            const auto hovered = active && node == hoveredAnchorNode;
            appendAnchorNode(node, selected ? q->anchorSelectedColor() : nodeColor,
                             selected || hovered);
        }
    }

    void appendAnchorPreview(const double localStart, const double localEnd) {
        if (!anchorEditing || anchorDragging || !anchorCursorInView)
            return;

        if (showAnchorMergePreview && currentAnchorCurve && anchorMergeCandidateCurve) {
            auto previewColor = q->anchorPreviewColor();
            previewColor.setAlpha(PitchDisplayStrategy::anchorInteractionPreviewAlpha());
            auto nodes = currentAnchorCurve->nodes().toList();
            nodes.append(anchorMergeCandidateCurve->nodes().toList());
            std::sort(nodes.begin(), nodes.end(),
                      [](const AnchorNode *left, const AnchorNode *right) {
                          return left->pos() < right->pos();
                      });
            appendAnchorStroke(nodes, localStart, localEnd, previewColor, true);
            return;
        }
        if (!showAnchorPreview || !anchorPreviewCurve)
            return;

        auto previewColor = q->anchorPreviewColor();
        previewColor.setAlpha(PitchDisplayStrategy::anchorPreviewAlpha());
        auto nodes = anchorPreviewCurve->nodes().toList();
        const auto point = anchorPointAt(anchorPreviewPosition);
        AnchorNode virtualNode(point.x(), point.y());
        if (std::any_of(nodes.cbegin(), nodes.cend(), [&virtualNode](const AnchorNode *node) {
                return node->pos() == virtualNode.pos();
            })) {
            return;
        }
        auto it = std::lower_bound(nodes.begin(), nodes.end(), &virtualNode,
                                   [](const AnchorNode *left, const AnchorNode *right) {
                                       return left->pos() < right->pos();
                                   });
        const auto insertIndex = static_cast<int>(it - nodes.begin());
        nodes.insert(it, &virtualNode);

        AnchorNode *oldLast = nullptr;
        AnchorNode::InterpMode savedLastMode = AnchorNode::Hermite;
        if (insertIndex == nodes.size() - 1) {
            virtualNode.setInterpMode(AnchorNode::None);
            if (nodes.size() > 1) {
                oldLast = nodes.at(nodes.size() - 2);
                savedLastMode = oldLast->interpMode();
                if (savedLastMode == AnchorNode::None) {
                    const auto predecessorMode = nodes.size() > 2
                                                     ? nodes.at(nodes.size() - 3)->interpMode()
                                                     : AnchorNode::Hermite;
                    oldLast->setInterpMode(predecessorMode);
                }
            }
        } else {
            auto mode = AnchorNode::Hermite;
            for (int i = insertIndex - 1; i >= 0; --i) {
                if (nodes.at(i)->pos() < virtualNode.pos()) {
                    mode = nodes.at(i)->interpMode();
                    break;
                }
            }
            virtualNode.setInterpMode(mode);
        }
        appendAnchorStroke(nodes, localStart, localEnd, previewColor, true);
        appendAnchorNode(&virtualNode, previewColor, false);
        if (oldLast)
            oldLast->setInterpMode(savedLastMode);
    }

    void appendAnchorDragPreview(const double localStart, const double localEnd) {
        if (!anchorDragging || anchorDragInfos.isEmpty())
            return;
        QSet<AnchorCurve *> targets;
        for (const auto &info : anchorDragInfos) {
            if (info.targetCurve)
                targets.insert(info.targetCurve);
        }
        auto previewColor = q->anchorPreviewColor();
        previewColor.setAlpha(PitchDisplayStrategy::anchorInteractionPreviewAlpha());
        for (auto *target : targets) {
            auto nodes = target->nodes().toList();
            QList<AnchorNode *> dragged;
            for (const auto &info : anchorDragInfos) {
                if (info.targetCurve != target)
                    continue;
                auto it = std::lower_bound(nodes.begin(), nodes.end(), info.node,
                                           [](const AnchorNode *left, const AnchorNode *right) {
                                               return left->pos() < right->pos();
                                           });
                nodes.insert(it, info.node);
                dragged.append(info.node);
            }
            appendAnchorStroke(nodes, localStart, localEnd, previewColor, true);
            for (auto *node : dragged)
                appendAnchorNode(node, previewColor, false);
        }
    }

    void appendAnchorSelectionRect() {
        if (!anchorSelecting)
            return;
        const auto sceneRect = anchorSelectionRect.normalized();
        auto fill = q->anchorPreviewColor();
        fill.setAlpha(PitchDisplayStrategy::anchorSelectionFillAlpha());
        auto border = q->anchorPreviewColor();
        border.setAlpha(PitchDisplayStrategy::anchorSelectionBorderAlpha());
        appendLogicalRect(sceneRect, fill);
        appendLine(sceneRect.topLeft(), sceneRect.topRight(), 1.5, border);
        appendLine(sceneRect.topRight(), sceneRect.bottomRight(), 1.5, border);
        appendLine(sceneRect.bottomRight(), sceneRect.bottomLeft(), 1.5, border);
        appendLine(sceneRect.bottomLeft(), sceneRect.topLeft(), 1.5, border);
    }

    void appendAnchors(const double localStart, const double localEnd) {
        const auto mode = PitchDisplayStrategy::displayModeForEditMode(editMode);
        const auto active = mode == PitchDisplayMode::Anchor;
        auto nodeColor = q->anchorColor();
        auto curveColor = q->anchorCurveColor();
        const auto opacity = PitchDisplayStrategy::anchorOpacity(mode);
        nodeColor.setAlpha(std::min(nodeColor.alpha(), opacity.nodeMaximumAlpha));
        curveColor.setAlpha(std::min(curveColor.alpha(), opacity.curveMaximumAlpha));
        if (active) {
            appendAnchorPreview(localStart, localEnd);
            appendAnchorDragPreview(localStart, localEnd);
        }
        const auto state = anchorOverlayState();
        for (auto *curve : anchorCurves)
            appendAnchorCurve(curve, localStart, localEnd, curveColor, nodeColor, active, state);
        if (active)
            appendAnchorSelectionRect();
    }

    void appendClipMask(const double localStart, const double localEnd, const double sceneTop,
                        const double sceneBottom) {
        const auto color = q->clipRangeOverlayColor();
        const auto clipStart = static_cast<double>(clip->clipStart());
        const auto clipEnd = clipStart + clip->clipLen();
        if (localStart < clipStart) {
            const auto end = std::min(localEnd, clipStart);
            appendLogicalRect(QRectF(localStart * pixelsPerTick(), sceneTop,
                                     (end - localStart) * pixelsPerTick(), sceneBottom - sceneTop),
                              color);
        }
        if (localEnd > clipEnd) {
            const auto start = std::max(localStart, clipEnd);
            appendLogicalRect(QRectF(start * pixelsPerTick(), sceneTop,
                                     (localEnd - start) * pixelsPerTick(), sceneBottom - sceneTop),
                              color);
        }
    }

    void appendLastPlaybackIndicator(const double sceneTop, const double sceneBottom) {
        const auto lastX = (lastPlaybackPosition - clip->start()) * pixelsPerTick();
        for (auto top = sceneTop; top < sceneBottom; top += 6.0) {
            appendPixelAlignedVerticalLine(lastX, top, std::min(top + 4.0, sceneBottom),
                                           q->lastPlayPosIndicatorColor());
        }
    }

    void appendRubberBand() {
        if (interaction != Interaction::RectSelect)
            return;
        auto sceneRect = QRectF(rubberBandStart, rubberBandEnd).normalized();
        const auto intervalSelect = editMode == IntervalSelect;
        if (intervalSelect) {
            sceneRect.setTop(0.0);
            sceneRect.setBottom(sceneHeight());
        }
        if (intervalSelect) {
            appendLogicalRect(sceneRect, q->rubberBandFillColor());
            appendLine(sceneRect.topLeft(), sceneRect.bottomLeft(), 1.5,
                       q->rubberBandBorderColor());
            appendLine(sceneRect.topRight(), sceneRect.bottomRight(), 1.5,
                       q->rubberBandBorderColor());
        } else {
            const auto physicalRect = QRectF(sceneRect.topLeft() * dpr, sceneRect.size() * dpr);
            const auto radius =
                EditorItemGeometry::adaptiveCornerRadius(physicalRect, 6.0 * dpr);
            EditorRhiGeometry::appendRoundedRect(vertices, physicalRect, radius,
                                                 q->rubberBandFillColor());
            EditorRhiGeometry::appendRoundedRectStroke(vertices, physicalRect, radius, 1.5 * dpr,
                                                       q->rubberBandBorderColor(), 0.5);
        }
    }

    void appendSplitPreview() {
        if (editMode != SplitNote || splitPreviewNoteId < 0 || !clip)
            return;
        const auto *note = clip->findNoteById(splitPreviewNoteId);
        if (!note || splitPreviewTick <= note->localStart() ||
            splitPreviewTick >= note->localStart() + note->length()) {
            return;
        }

        constexpr double extensionLength = 8.0;
        constexpr double forkLength = 6.0;
        constexpr double forkAngle = 45.0;
        constexpr double lineWidth = 2.0;
        const auto x = splitPreviewTick * pixelsPerTick();
        const auto noteTop = viewport.unitToSceneY(127 - note->keyIndex());
        const auto lineTop = noteTop - extensionLength;
        const auto lineBottom = noteTop + noteHeight * verticalScale() + extensionLength;
        const auto forkAngleRad = forkAngle * std::numbers::pi / 180.0;
        const auto forkOffsetX = forkLength * std::sin(forkAngleRad);
        const auto forkOffsetY = forkLength * std::cos(forkAngleRad);
        const auto color = q->splitLineColor();

        appendLine({x, lineTop}, {x, lineBottom}, lineWidth, color);
        appendLine({x, lineTop}, {x - forkOffsetX, lineTop - forkOffsetY}, lineWidth, color);
        appendLine({x, lineTop}, {x + forkOffsetX, lineTop - forkOffsetY}, lineWidth, color);
        appendLine({x, lineBottom}, {x - forkOffsetX, lineBottom + forkOffsetY}, lineWidth, color);
        appendLine({x, lineBottom}, {x + forkOffsetX, lineBottom + forkOffsetY}, lineWidth, color);
    }

public:
    void requestFallback() {
        if (fallbackRequested)
            return;
        fallbackRequested = true;
        QMetaObject::invokeMethod(
            q, [this] { q->notifyBackendUnavailable(); }, Qt::QueuedConnection);
    }

    PianoRollRhiWidget *q;
    QPointer<SingingClip> clip;
    bool cameraInitialized = false;
    bool viewportPositionPending = false;
    bool snapshotScheduled = false;
    bool fallbackRequested = false;
    int trackColorIndex = 0;
    PianoRollEditMode editMode = Select;
    EditorSelectionUtils::OrderedSelectionModel noteSelection;
    Interaction interaction = Interaction::None;
    int interactionNoteId = -1;
    int interactionStart = 0;
    int interactionLength = 0;
    int interactionKey = 60;
    int interactionDeltaTick = 0;
    int interactionDeltaKey = 0;
    int interactionMinimumLength = 1;
    bool interactionMoved = false;
    quint64 noteEditSessionId = 0;
    double mouseDownTick = 0.0;
    int mouseDownKey = 60;
    int drawStart = 0;
    int drawEnd = 0;
    int drawKey = 60;
    int hoveredKey = -1;
    int splitPreviewNoteId = -1;
    int splitPreviewTick = 0;
    QPointF rubberBandStart;
    QPointF rubberBandEnd;
    QList<int> rubberBandBaseSelection;
    QList<PastePreviewNote> pastePreviewNotes;
    enum class PitchEditType { None, DrawOnCurve, DrawOnInterval, Erase };
    PitchEditType pitchEditType = PitchEditType::None;
    bool pitchEditing = false;
    bool pitchMouseMoved = false;
    bool pitchNewCurveCreated = false;
    quint64 pitchEditSessionId = 0;
    QPoint pitchMouseDownPos;
    QPoint pitchPreviousPos;
    QList<DrawCurve *> pitchPreviewCurves;
    DrawCurve *pitchEditingCurve = nullptr;
    PitchDisplayStrategy::MergedCurveCache mergedPitchCurveCache;
    bool noteErasing = false;
    QList<int> erasedNoteIds;
    quint64 noteEraseSessionId = 0;
    QList<AnchorCurve *> anchorCurves;
    quint64 anchorEditSessionId = 0;
    bool anchorCommitting = false;
    bool anchorEditing = false;
    bool anchorDragging = false;
    bool anchorSelecting = false;
    bool anchorCursorInView = true;
    AnchorCurve *currentAnchorCurve = nullptr;
    QList<AnchorNode *> selectedAnchorNodes;
    AnchorNode *hoveredAnchorNode = nullptr;
    QList<AnchorEditor::DragNodeInfo> anchorDragInfos;
    QRectF anchorSelectionRect;
    QPointF anchorPreviewPosition;
    QPoint anchorDragStartPoint;
    QPointF anchorDragPressViewportPos;
    AnchorCurve *anchorPreviewCurve = nullptr;
    AnchorCurve *anchorMergeCandidateCurve = nullptr;
    AnchorNode *anchorMergeEndpointNode = nullptr;
    bool showAnchorPreview = false;
    bool showAnchorMergePreview = false;
    EditorViewportController viewport;
    EditorWheelController wheel;
    double playbackPosition = 0.0;
    double lastPlaybackPosition = 0.0;
    bool autoPageTurn = true;
    bool autoPageTurnAvailable = false;
    double dpr = 1.0;
    QVector<Vertex> vertices;
    QVector<EditorRhiOverlayRect> playbackOverlayRects;
    TimelineLineEmitter timelineEmitter;
    EditorGlyphAtlas glyphAtlas;
    EditorRhiDrawList drawList;
    EdgeAutoScroller edgeAutoScroller;
    int noteFontPixelSize = 13;
    QColor whiteKeyColor{38, 40, 44};
    QColor blackKeyColor{31, 33, 37};
    QColor octaveDividerColor{56, 59, 65};
    QColor noteSelectedBorderColor{255, 255, 255};
    QColor pronunciationTextColor{200, 200, 200};
    QColor clipRangeOverlayColor{0, 0, 0, 90};
    QColor paramOriginalCurveColor{120, 170, 210, 180};
    QColor paramEditedCurveColor{90, 205, 180, 210};
    QColor anchorColor{220, 220, 220};
    QColor anchorSelectedColor{255, 205, 80};
    QColor anchorCurveColor{220, 220, 220};
    QColor anchorPreviewColor{120, 180, 255};
    QColor splitLineColor{255, 100, 100};
    QColor barLineColor{86, 90, 98};
    QColor beatLineColor{62, 66, 73};
    QColor commonLineColor{47, 50, 56};
    QColor playPosIndicatorColor{200, 200, 200};
    QColor lastPlayPosIndicatorColor{160, 160, 160};
    QColor rubberBandBorderColor{155, 186, 255, 200};
    QColor rubberBandFillColor{155, 186, 255, 64};
    InlineTextEditOverlay *inlineEditor = nullptr;
    EditorRhiScrollBarController *scrollBars = nullptr;
    InlineEditField inlineEditField = InlineEditField::None;
    int inlineEditingNoteId = -1;
};

PianoRollRhiWidget::PianoRollRhiWidget(QWidget *parent)
    : EditorRhiWidget(QStringLiteral("PianoRollRhi"), parent), d(std::make_unique<Private>(this)) {
    setObjectName(QStringLiteral("PianoRollRhiWidget"));
    setMouseTracking(true);
    d->initializeScrollBars();
    d->initializeInlineEditor();
    connect(this, &EditorRhiWidget::backendFailed, this,
            [this](const QString &) { d->requestFallback(); });
    connect(appStatus, &AppStatus::pianoRollQuantizeChanged, this,
            [this] { d->scheduleSnapshot(); });
    connect(appStatus, &AppStatus::noteSelectionChanged, this, [this](const QList<int> &selection) {
        d->noteSelection.synchronize(selection);
        d->scheduleSnapshot();
    });
    d->noteSelection.synchronize(appStatus->selectedNotes.get());
    connect(appModel, &AppModel::timelineChanged, this, [this] {
        d->updateAutoPageTurnAvailability();
        d->scheduleSnapshot();
    });
}

PianoRollRhiWidget::~PianoRollRhiWidget() {
    d->discardNoteInteraction();
    d->finishNoteErase(EditSessionEndReason::Discard);
    d->cancelPitchEdit(false);
    d->finishAnchorEditSession(EditSessionEndReason::Discard);
}

void PianoRollRhiWidget::setDataContext(SingingClip *clip) {
    d->setDataContext(clip);
}

void PianoRollRhiWidget::setTrackColorIndex(const int index) {
    d->setTrackColorIndex(index);
}

double PianoRollRhiWidget::startTick() const {
    return d->startTick();
}

double PianoRollRhiWidget::endTick() const {
    return d->endTick();
}

double PianoRollRhiWidget::topKeyIndex() const {
    return d->topKeyIndex();
}

double PianoRollRhiWidget::bottomKeyIndex() const {
    return d->bottomKeyIndex();
}

double PianoRollRhiWidget::centerKeyIndex() const {
    return d->centerKeyIndex();
}

double PianoRollRhiWidget::scaleX() const {
    return d->horizontalScale();
}

double PianoRollRhiWidget::scaleY() const {
    return d->verticalScale();
}

int PianoRollRhiWidget::horizontalBarValue() const {
    return d->horizontalBarValue();
}

PianoRollViewState PianoRollRhiWidget::viewState() const {
    return {(startTick() + endTick()) * 0.5, centerKeyIndex(), scaleX(), scaleY(), d->editMode};
}

bool PianoRollRhiWidget::centerAt(const double tick, const double keyIndex) {
    return d->centerAt(tick, keyIndex);
}

bool PianoRollRhiWidget::setViewScale(const double horizontalScale, const double verticalScale) {
    return d->setViewScale(horizontalScale, verticalScale);
}

HistoryFocusVisibility PianoRollRhiWidget::focusVisibility(const HistoryFocus &focus) const {
    return d->focusVisibility(focus);
}

bool PianoRollRhiWidget::revealFocus(const HistoryFocus &focus, const bool animated) {
    return d->revealFocus(focus, animated);
}

void PianoRollRhiWidget::setEditMode(const PianoRollEditMode mode) {
    if (d->editMode != mode) {
        setCursor(Qt::ArrowCursor);
        d->disarmEdgeAutoScroll();
        d->discardNoteInteraction();
        d->finishNoteErase(EditSessionEndReason::Discard);
        d->finishInlineEditing();
        d->cancelPitchEdit();
        if (d->editMode == EditPitchAnchor)
            d->cancelAnchorEdit();
        if (d->editMode == SplitNote)
            d->clearSplitPreview();
        if (mode == EditPitchAnchor) {
            d->loadAnchorCurvesFromModel();
            d->anchorCursorInView = true;
        }
    }
    d->editMode = mode;
    d->scheduleSnapshot();
}

void PianoRollRhiWidget::onWheelHorScale(QWheelEvent *event) {
    d->horizontalScale(event);
}

void PianoRollRhiWidget::onWheelVerScale(QWheelEvent *event) {
    d->verticalScale(event);
}

void PianoRollRhiWidget::onWheelHorScroll(QWheelEvent *event) {
    d->horizontalScroll(event);
}

void PianoRollRhiWidget::onWheelVerScroll(QWheelEvent *event) {
    d->verticalScroll(event);
}

void PianoRollRhiWidget::setHorizontalBarValue(const int value) {
    d->setHorizontalBarValue(value);
}

void PianoRollRhiWidget::setAutoPageTurn(const bool enabled) {
    d->setAutoPageTurn(enabled);
}

void PianoRollRhiWidget::setPlaybackPosition(const double tick) {
    d->setPlaybackPosition(tick);
}

void PianoRollRhiWidget::setLastPlaybackPosition(const double tick) {
    d->lastPlaybackPosition = tick;
    d->scheduleSnapshot();
}

void PianoRollRhiWidget::showEvent(QShowEvent *event) {
    EditorRhiWidget::showEvent(event);
    d->show();
    d->updateAutoPageTurnAvailability();
}

bool PianoRollRhiWidget::event(QEvent *event) {
    if (event->type() == QEvent::WindowDeactivate) {
        d->disarmEdgeAutoScroll();
        d->discardNoteInteraction();
        d->finishNoteErase(EditSessionEndReason::Discard);
        d->cancelPitchEdit();
        if (d->editMode == EditPitchAnchor)
            d->cancelAnchorEdit();
    }
    if (d->clip && event->type() == QEvent::NativeGesture &&
        d->wheel.handleNativeGesture(static_cast<QNativeGestureEvent *>(event))) {
        return true;
    }
    return EditorRhiWidget::event(event);
}

void PianoRollRhiWidget::hideEvent(QHideEvent *event) {
    d->disarmEdgeAutoScroll();
    d->discardNoteInteraction();
    d->finishNoteErase(EditSessionEndReason::Discard);
    EditorRhiWidget::hideEvent(event);
    d->updateAutoPageTurnAvailability();
}

void PianoRollRhiWidget::resizeEvent(QResizeEvent *event) {
    EditorRhiWidget::resizeEvent(event);
    d->resize();
}

void PianoRollRhiWidget::wheelEvent(QWheelEvent *event) {
    if (!d->wheel.handleWheel(event))
        EditorRhiWidget::wheelEvent(event);
}

void PianoRollRhiWidget::mousePressEvent(QMouseEvent *event) {
    d->wheel.stop();
    setFocus(Qt::MouseFocusReason);
    if (event->button() == Qt::LeftButton)
        d->prepareEdgeAutoScroll(event->position());
    d->mousePress(event);
    event->accept();
}

void PianoRollRhiWidget::mouseMoveEvent(QMouseEvent *event) {
    d->mouseMove(event);
    d->updateEdgeAutoScrollState(event->position());
    event->accept();
}

void PianoRollRhiWidget::mouseReleaseEvent(QMouseEvent *event) {
    d->mouseRelease(event);
    d->disarmEdgeAutoScroll();
    event->accept();
}

void PianoRollRhiWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if (d->clip && d->editMode == EditPitchAnchor && event->button() == Qt::LeftButton) {
        setFocus(Qt::MouseFocusReason);
        d->createAnchorAt(event->position());
        d->anchorDragStartPoint = d->anchorPointAt(event->position());
        d->anchorDragPressViewportPos = event->position();
        d->prepareEdgeAutoScroll(event->position());
        d->anchorDragging = false;
        d->anchorDragInfos.clear();
        d->scheduleSnapshot();
        event->accept();
        return;
    }
    const auto supportsInlineEditing =
        d->editMode == Select || d->editMode == IntervalSelect || d->editMode == DrawNote;
    if (d->clip && supportsInlineEditing && event->button() == Qt::LeftButton) {
        d->interaction = Private::Interaction::None;
        d->interactionNoteId = -1;
        if (auto *note = d->pronunciationAt(event->position())) {
            d->startPronunciationEdit(note);
            event->accept();
            return;
        }
        if (auto *note = d->noteAt(event->position())) {
            d->startLyricEdit(note);
            event->accept();
            return;
        }
        if (d->editMode == Select) {
            d->beginDrawNote(event->position());
            d->prepareEdgeAutoScroll(event->position());
            event->accept();
            return;
        }
    }
    EditorRhiWidget::mouseDoubleClickEvent(event);
}

void PianoRollRhiWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape)
        d->disarmEdgeAutoScroll();
    if (d->editMode == EditPitchAnchor && d->keyPressAnchor(event)) {
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        if (d->noteErasing) {
            d->finishNoteErase(EditSessionEndReason::Discard);
            event->accept();
            return;
        }
        if (d->pitchEditing) {
            d->cancelPitchEdit();
            event->accept();
            return;
        }
        d->discardNoteInteraction();
        event->accept();
        return;
    }
    EditorRhiWidget::keyPressEvent(event);
}

void PianoRollRhiWidget::leaveEvent(QEvent *event) {
    unsetCursor();
    if (d->hoveredKey >= 0) {
        d->hoveredKey = -1;
        emit keyHoverCleared();
    }
    if (d->editMode == EditPitchAnchor) {
        d->anchorCursorInView = false;
        d->hoveredAnchorNode = nullptr;
        d->showAnchorPreview = false;
        d->showAnchorMergePreview = false;
        d->scheduleSnapshot();
    }
    if (d->editMode == SplitNote)
        d->clearSplitPreview();
    EditorRhiWidget::leaveEvent(event);
}

void PianoRollRhiWidget::contextMenuEvent(QContextMenuEvent *event) {
    if (!d->clip)
        return;

    if (EditorViewGlobal::isPitchEditMode(d->editMode) && d->editMode != EditPitchAnchor) {
        event->accept();
        return;
    }

    PianoRollMenuContext context;
    context.globalPos = event->globalPos();
    context.globalTick = qRound(d->localTickAt(event->pos())) + d->clip->start();
    context.keyIndex = d->keyAt(event->pos());

    if (d->editMode == EditPitchAnchor) {
        auto *node = d->anchorNodeAt(event->pos());
        if (!node) {
            if (d->anchorEditing) {
                d->exitAnchorEditing();
                d->scheduleSnapshot();
            }
            event->accept();
            return;
        }

        if (d->selectedAnchorNodes.size() <= 1 || !d->selectedAnchorNodes.contains(node)) {
            d->clearAnchorSelection();
            d->selectAnchorNode(node);
            d->enterAnchorEditing(d->currentAnchorCurve, node);
        }
        d->scheduleSnapshot();

        const auto currentMode = d->selectedAnchorNodes.first()->interpMode();
        const auto allSame =
            std::all_of(d->selectedAnchorNodes.begin(), d->selectedAnchorNodes.end(),
                        [currentMode](const AnchorNode *selected) {
                            return selected->interpMode() == currentMode;
                        });
        const auto isLastNode = [this](const AnchorNode *selected) {
            const auto *owner = d->findAnchorOwner(selected);
            const auto nodes = owner ? owner->nodes().toList() : QList<AnchorNode *>();
            return !nodes.isEmpty() && nodes.last() == selected;
        };
        const auto allAreLast =
            std::all_of(d->selectedAnchorNodes.begin(), d->selectedAnchorNodes.end(), isLastNode);
        context.target = PianoRollMenuContext::Target::Anchor;
        context.anchorInterpolationEnabled = !allAreLast;
        context.anchorMode = !allSame                             ? PianoRollAnchorMode::Mixed
                             : currentMode == AnchorNode::Linear  ? PianoRollAnchorMode::Linear
                             : currentMode == AnchorNode::Hermite ? PianoRollAnchorMode::Hermite
                                                                  : PianoRollAnchorMode::None;
        emit contextMenuRequested(context);
        event->accept();
        return;
    }

    const auto target = d->noteContextTargetAt(event->pos());
    if (auto *note = target.note) {
        d->updateContextNoteSelection(note);
        context.target = PianoRollMenuContext::Target::Note;
        context.noteId = note->id();
        context.selectedNoteIds = appStatus->selectedNotes.get();
        context.noteLanguage = note->language();
        context.phonemeEditorEnabled = context.selectedNoteIds.size() == 1;
        // Right-click on the pronunciation strip opens the quick-switch menu.
        context.pronunciationTarget = target.pronunciation;
    } else {
        context.target = PianoRollMenuContext::Target::Background;
    }
    emit contextMenuRequested(context);
    event->accept();
}

void PianoRollRhiWidget::showPianoRollPastePreview(const PianoRollPastePreviewData &data,
                                                   const int globalTick) {
    d->setPastePreview(data, globalTick);
}

void PianoRollRhiWidget::clearPianoRollPastePreview() {
    d->clearPastePreview();
}

void PianoRollRhiWidget::setSelectedAnchorInterpolation(const PianoRollAnchorMode mode) {
    if (mode != PianoRollAnchorMode::Linear && mode != PianoRollAnchorMode::Hermite)
        return;
    const auto targetMode =
        mode == PianoRollAnchorMode::Linear ? AnchorNode::Linear : AnchorNode::Hermite;
    bool changed = false;
    for (auto *selected : d->selectedAnchorNodes) {
        const auto *owner = d->findAnchorOwner(selected);
        const auto nodes = owner ? owner->nodes().toList() : QList<AnchorNode *>();
        if (!nodes.isEmpty() && nodes.last() != selected && selected->interpMode() != targetMode) {
            selected->setInterpMode(targetMode);
            changed = true;
        }
    }
    if (changed)
        d->commitAnchorEdit();
}

void PianoRollRhiWidget::deleteSelectedAnchors() {
    d->deleteSelectedAnchorNodes();
}

void PianoRollRhiWidget::onRhiReady() {
    d->scheduleSnapshot();
}

void PianoRollRhiWidget::onDevicePixelRatioChanged() {
    EditorRhiWidget::onDevicePixelRatioChanged();
    d->glyphAtlas.clear();
    d->scheduleSnapshot();
}

void PianoRollRhiWidget::notifyViewportChanged() {
    d->updateScrollBars();
    emit timeRangeChanged(startTick(), endTick());
    emit keyRangeChanged(topKeyIndex(), bottomKeyIndex());
    emit scaleChanged(scaleX(), scaleY());
    emit horizontalBarValueChanged(horizontalBarValue());
    d->updateAutoPageTurnAvailability();
}

void PianoRollRhiWidget::notifyBackendUnavailable() {
    emit backendUnavailable();
}

int PianoRollRhiWidget::noteFontPixelSize() const {
    return d->noteFontPixelSize;
}

void PianoRollRhiWidget::setNoteFontPixelSize(const int size) {
    if (d->noteFontPixelSize == size)
        return;
    d->noteFontPixelSize = size;
    d->glyphAtlas.clear();
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::whiteKeyColor() const {
    return d->whiteKeyColor;
}

void PianoRollRhiWidget::setWhiteKeyColor(const QColor &color) {
    d->whiteKeyColor = color;
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::blackKeyColor() const {
    return d->blackKeyColor;
}

void PianoRollRhiWidget::setBlackKeyColor(const QColor &color) {
    d->blackKeyColor = color;
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::octaveDividerColor() const {
    return d->octaveDividerColor;
}

void PianoRollRhiWidget::setOctaveDividerColor(const QColor &color) {
    d->octaveDividerColor = color;
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::noteSelectedBorderColor() const {
    return d->noteSelectedBorderColor;
}

void PianoRollRhiWidget::setNoteSelectedBorderColor(const QColor &color) {
    d->noteSelectedBorderColor = color;
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::pronunciationTextColor() const {
    return d->pronunciationTextColor;
}

void PianoRollRhiWidget::setPronunciationTextColor(const QColor &color) {
    d->pronunciationTextColor = color;
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::clipRangeOverlayColor() const {
    return d->clipRangeOverlayColor;
}

void PianoRollRhiWidget::setClipRangeOverlayColor(const QColor &color) {
    d->clipRangeOverlayColor = color;
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::paramOriginalCurveColor() const {
    return d->paramOriginalCurveColor;
}

void PianoRollRhiWidget::setParamOriginalCurveColor(const QColor &color) {
    d->paramOriginalCurveColor = color;
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::paramEditedCurveColor() const {
    return d->paramEditedCurveColor;
}

void PianoRollRhiWidget::setParamEditedCurveColor(const QColor &color) {
    d->paramEditedCurveColor = color;
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::anchorColor() const {
    return d->anchorColor;
}

void PianoRollRhiWidget::setAnchorColor(const QColor &color) {
    d->anchorColor = color;
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::anchorSelectedColor() const {
    return d->anchorSelectedColor;
}

void PianoRollRhiWidget::setAnchorSelectedColor(const QColor &color) {
    d->anchorSelectedColor = color;
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::anchorCurveColor() const {
    return d->anchorCurveColor;
}

void PianoRollRhiWidget::setAnchorCurveColor(const QColor &color) {
    d->anchorCurveColor = color;
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::anchorPreviewColor() const {
    return d->anchorPreviewColor;
}

void PianoRollRhiWidget::setAnchorPreviewColor(const QColor &color) {
    d->anchorPreviewColor = color;
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::splitLineColor() const {
    return d->splitLineColor;
}

void PianoRollRhiWidget::setSplitLineColor(const QColor &color) {
    if (d->splitLineColor == color)
        return;
    d->splitLineColor = color;
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::barLineColor() const {
    return d->barLineColor;
}

void PianoRollRhiWidget::setBarLineColor(const QColor &color) {
    d->barLineColor = color;
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::beatLineColor() const {
    return d->beatLineColor;
}

void PianoRollRhiWidget::setBeatLineColor(const QColor &color) {
    d->beatLineColor = color;
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::commonLineColor() const {
    return d->commonLineColor;
}

void PianoRollRhiWidget::setCommonLineColor(const QColor &color) {
    d->commonLineColor = color;
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::playPosIndicatorColor() const {
    return d->playPosIndicatorColor;
}

void PianoRollRhiWidget::setPlayPosIndicatorColor(const QColor &color) {
    d->playPosIndicatorColor = color;
    d->updatePlaybackOverlay();
}

QColor PianoRollRhiWidget::lastPlayPosIndicatorColor() const {
    return d->lastPlayPosIndicatorColor;
}

void PianoRollRhiWidget::setLastPlayPosIndicatorColor(const QColor &color) {
    d->lastPlayPosIndicatorColor = color;
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::rubberBandBorderColor() const {
    return d->rubberBandBorderColor;
}

void PianoRollRhiWidget::setRubberBandBorderColor(const QColor &color) {
    d->rubberBandBorderColor = color;
    d->scheduleSnapshot();
}

QColor PianoRollRhiWidget::rubberBandFillColor() const {
    return d->rubberBandFillColor;
}

void PianoRollRhiWidget::setRubberBandFillColor(const QColor &color) {
    d->rubberBandFillColor = color;
    d->scheduleSnapshot();
}
