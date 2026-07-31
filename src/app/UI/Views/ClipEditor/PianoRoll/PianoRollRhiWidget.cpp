#include "PianoRollRhiWidget.h"

#include "NoteView.h"
#include "PianoPaintUtils.h"
#include "PianoRollGraphicsViewHelper.h"
#include "PronunciationView.h"
#include "Controller/ClipController.h"
#include "Global/AppGlobal.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Utils/AppColorPalette.h"
#include "UI/Utils/ITimelinePainter.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/Common/EditorRhiGeometry.h"
#include "UI/Views/Common/EditorGlyphAtlas.h"
#include "Global/ControllerGlobal.h"
#include "Modules/Inference/EditSessionManager.h"

#include <lite/GUI/Controls/Menu.h>
#include <lite/GUI/Controls/InlineTextEditOverlay.h>
#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/Utils/AppModelUtils.h>
#include <lite/Support/MathUtils.h>
#include <lite/MusicBase/TimelineSnapUtils.h>

#include <QEvent>
#include <QActionGroup>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QHash>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLineF>
#include <QMetaObject>
#include <QMouseEvent>
#include <QMimeData>
#include <QPainter>
#include <QResizeEvent>
#include <QSet>
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
    constexpr float kNoteBorderWidth = 1.5f;

    using Vertex = EditorRhiSolidVertex;

    QColor blendedColor(const QColor &from, const QColor &to, const double ratio) {
        const auto t = std::clamp(ratio, 0.0, 1.0);
        return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * t,
                                from.greenF() + (to.greenF() - from.greenF()) * t,
                                from.blueF() + (to.blueF() - from.blueF()) * t,
                                from.alphaF() + (to.alphaF() - from.alphaF()) * t);
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

    struct AnchorDragInfo {
        AnchorNode *node = nullptr;
        AnchorCurve *sourceCurve = nullptr;
        AnchorCurve *targetCurve = nullptr;
        int startTick = 0;
        int startValue = 0;
    };

    explicit Private(PianoRollRhiWidget *q) : q(q) {
    }

    ~Private() {
        clearPitchPreview();
        qDeleteAll(anchorCurves);
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

    void setDataContext(SingingClip *newClip) {
        finishInlineEditing();
        cancelPitchEdit();
        cancelAnchorEdit(false);
        clearSplitPreview();
        if (clip)
            QObject::disconnect(clip, nullptr, q, nullptr);

        clip = newClip;
        cameraInitialized = false;
        if (clip) {
            QObject::connect(clip, &SingingClip::noteChanged, q, [this] { scheduleSnapshot(); });
            QObject::connect(clip, &SingingClip::paramChanged, q,
                             [this](const ParamInfo::Name name, Param::Type) {
                                 if (name == ParamInfo::Pitch) {
                                     if (!anchorCommitting)
                                         loadAnchorCurvesFromModel();
                                     scheduleSnapshot();
                                 }
                             });
            QObject::connect(clip, &SingingClip::propertyChanged, q,
                             [this] { scheduleSnapshot(); });
        }
        loadAnchorCurvesFromModel();
        initializeCamera();
        scheduleSnapshot();
    }

    void initializeCamera() {
        if (!clip || q->width() <= 0 || q->height() <= 0)
            return;

        scaleX = std::max(1.0, minimumScaleX());
        scaleY = std::max(1.0, minimumScaleY());
        cameraX = 0.0;
        cameraY = 0.0;
        if (clip->notes().count() > 0) {
            const auto *firstNote = *clip->notes().begin();
            const auto visibleTicks = q->width() / pixelsPerTick();
            cameraX = (firstNote->localStart() - visibleTicks * 0.3) * pixelsPerTick();
            const auto noteCenterY = (127.5 - firstNote->keyIndex()) * noteHeight * scaleY;
            cameraY = noteCenterY - q->height() * 0.5;
        }
        clampCamera();
        cameraInitialized = true;
        q->notifyViewportChanged();
    }

    void resize() {
        if (!cameraInitialized) {
            initializeCamera();
        } else {
            scaleX = std::max(scaleX, minimumScaleX());
            scaleY = std::max(scaleY, minimumScaleY());
            clampCamera();
            q->notifyViewportChanged();
        }
        scheduleSnapshot();
    }

    void setTrackColorIndex(const int index) {
        trackColorIndex = index;
        scheduleSnapshot();
    }

    double startTick() const {
        return (clip ? clip->start() : 0) + visibleLocalStartTick();
    }

    double endTick() const {
        return (clip ? clip->start() : 0) + visibleLocalEndTick();
    }

    double topKeyIndex() const {
        return 127.0 - cameraY / (noteHeight * scaleY);
    }

    double bottomKeyIndex() const {
        return 127.0 - (cameraY + q->height()) / (noteHeight * scaleY);
    }

    double centerKeyIndex() const {
        return (topKeyIndex() + bottomKeyIndex()) * 0.5;
    }

    bool centerAt(const double tick, const double keyIndex) {
        if (!clip || !std::isfinite(tick) || !std::isfinite(keyIndex))
            return false;
        cameraX = (tick - clip->start()) * pixelsPerTick() - q->width() * 0.5;
        cameraY = (127.0 - keyIndex + 0.5) * noteHeight * scaleY - q->height() * 0.5;
        viewportChanged(false);
        return true;
    }

    bool setViewScale(const double horizontal, const double vertical) {
        if (!std::isfinite(horizontal) || !std::isfinite(vertical) || horizontal <= 0.0 ||
            vertical <= 0.0)
            return false;
        const auto centerTickValue = (startTick() + endTick()) * 0.5;
        const auto centerKey = centerKeyIndex();
        scaleX = std::clamp(horizontal, minimumScaleX(), 5.0);
        scaleY = std::clamp(vertical, minimumScaleY(), 8.0);
        centerAt(centerTickValue, centerKey);
        scheduleSnapshot();
        return true;
    }

    int horizontalBarValue() const {
        return qRound(cameraX);
    }

    void setHorizontalBarValue(const int value) {
        cameraX = value;
        viewportChanged(false);
    }

    HistoryFocusVisibility focusVisibility(const HistoryFocus &focus) const {
        if (!clip || focus.kind != HistoryFocusKind::PianoRollNotes || !focus.isValid())
            return HistoryFocusVisibility::Unavailable;
        if (focus.containerId >= 0 && focus.containerId != clip->id())
            return HistoryFocusVisibility::ContextSwitchRequired;
        const auto tickOffset = focus.ticksAreLocal ? clip->start() : 0.0;
        const auto tickVisible =
            focus.tickEnd + tickOffset >= startTick() && focus.tickStart + tickOffset <= endTick();
        const auto keyVisible =
            focus.valueEnd >= bottomKeyIndex() && focus.valueStart <= topKeyIndex();
        return tickVisible && keyVisible ? HistoryFocusVisibility::Visible
                                         : HistoryFocusVisibility::ScrollRequired;
    }

    bool revealFocus(const HistoryFocus &focus) {
        if (focusVisibility(focus) == HistoryFocusVisibility::Unavailable || !clip)
            return false;
        if (focus.containerId >= 0 && focus.containerId != clip->id())
            return false;
        QList<int> selected;
        for (const auto id : focus.objectIds)
            if (clip->findNoteById(id))
                selected.append(id);
        appStatus->selectedNotes = selected;
        const auto tickOffset = focus.ticksAreLocal ? clip->start() : 0.0;
        return centerAt((focus.tickStart + focus.tickEnd) * 0.5 + tickOffset,
                        (focus.valueStart + focus.valueEnd) * 0.5);
    }

    void horizontalScale(QWheelEvent *event) {
        if (!clip)
            return;
        const auto delta = wheelDelta(event, false);
        if (qFuzzyIsNull(delta))
            return;
        const auto oldScale = scaleX;
        auto target = delta > 0 ? oldScale * (1.0 + 0.4 * delta / 120.0)
                                : oldScale / (1.0 + 0.4 * -delta / 120.0);
        target = std::clamp(target, minimumScaleX(), 5.0);
        const auto anchor = event->position().x();
        cameraX = (cameraX + anchor) * target / oldScale - anchor;
        scaleX = target;
        viewportChanged(true);
    }

    void verticalScale(QWheelEvent *event) {
        if (!clip)
            return;
        const auto delta = wheelDelta(event, true);
        if (qFuzzyIsNull(delta))
            return;
        const auto oldScale = scaleY;
        auto target = delta > 0 ? oldScale * (1.0 + 0.3 * delta / 120.0)
                                : oldScale / (1.0 + 0.3 * -delta / 120.0);
        target = std::clamp(target, minimumScaleY(), 8.0);
        const auto anchor = event->position().y();
        cameraY = (cameraY + anchor) * target / oldScale - anchor;
        scaleY = target;
        viewportChanged(true);
    }

    void horizontalScroll(QWheelEvent *event) {
        const auto delta = wheelDelta(event, false);
        cameraX += -q->width() * 0.2 * delta / 120.0;
        viewportChanged(false);
    }

    void verticalScroll(QWheelEvent *event) {
        const auto delta = wheelDelta(event, false);
        cameraY += -q->height() * 0.15 * delta / 120.0;
        viewportChanged(false);
    }

    int keyAt(const QPointF &viewportPosition) const {
        const auto row =
            static_cast<int>(std::floor((cameraY + viewportPosition.y()) / (noteHeight * scaleY)));
        return std::clamp(127 - row, 0, 127);
    }

    double localTickAt(const QPointF &viewportPosition) const {
        return (cameraX + viewportPosition.x()) / std::max(0.0001, pixelsPerTick());
    }

    Note *noteAt(const QPointF &viewportPosition) const {
        if (!clip)
            return nullptr;
        const auto tick = localTickAt(viewportPosition);
        const auto key = keyAt(viewportPosition);
        for (auto iterator = clip->notes().rbegin(); iterator != clip->notes().rend(); ++iterator) {
            auto *note = *iterator;
            if (note->keyIndex() == key && tick >= note->localStart() &&
                tick <= note->localStart() + note->length())
                return note;
        }
        return nullptr;
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

    QRectF noteViewportRect(const Note *note) const {
        if (!note)
            return {};
        return {note->localStart() * pixelsPerTick() - cameraX,
                (127 - note->keyIndex()) * noteHeight * scaleY - cameraY,
                note->length() * pixelsPerTick(), noteHeight * scaleY};
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

    int snapLocalTick(const double localTick) const {
        const auto step = TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
        const auto globalTick = qRound(localTick) + (clip ? clip->start() : 0);
        return TimelineSnapUtils::snapNearest(globalTick, step, appModel->timeline()) -
               (clip ? clip->start() : 0);
    }

    QPoint pitchPointAt(const QPointF &viewportPosition) const {
        const auto tick = std::max(
            0, MathUtils::round(static_cast<int>(localTickAt(viewportPosition)), DrawCurve().step));
        const auto sceneY = cameraY + viewportPosition.y();
        const auto value = qRound((127.5 - sceneY / (noteHeight * scaleY)) * 100.0);
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
        const auto sceneY = cameraY + viewportPosition.y();
        const auto value = qRound((127.5 - sceneY / (noteHeight * scaleY)) * 100.0);
        return {tick, std::clamp(value, 0, 12700)};
    }

    QPointF anchorNodeViewportPosition(const AnchorNode *node) const {
        return {node->pos() * pixelsPerTick() - cameraX,
                (12700 - node->value() + 50) * scaleY * noteHeight / 100.0 - cameraY};
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
        anchorSelectionRect.setBottomRight(viewportPosition);
        const auto rect = anchorSelectionRect.normalized();
        clearAnchorSelection();
        for (auto *curve : anchorCurves) {
            for (auto *node : curve->nodes().toList()) {
                if (rect.contains(anchorNodeViewportPosition(node)))
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
        const auto deltaTick =
            static_cast<int>((viewportPosition.x() - anchorDragStart.x()) / pixelsPerTick());
        const auto deltaValue =
            anchorPointAt(viewportPosition).y() - anchorPointAt(anchorDragStart).y();
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
                    anchorDragStart = event->position();
                    anchorDragging = false;
                    anchorDragInfos.clear();
                }
            } else {
                createAnchorAt(event->position());
                anchorDragStart = event->position();
                anchorDragging = false;
                anchorDragInfos.clear();
            }
        } else if (node) {
            selectAnchorNode(node);
            enterAnchorEditing(currentAnchorCurve, node);
            anchorDragStart = event->position();
            anchorDragging = false;
            anchorDragInfos.clear();
        } else {
            anchorSelectionRect = QRectF(event->position(), QSizeF());
            anchorSelecting = true;
        }
        scheduleSnapshot();
    }

    void mouseMoveAnchor(QMouseEvent *event) {
        if (event->buttons().testFlag(Qt::LeftButton)) {
            if (anchorEditing && !selectedAnchorNodes.isEmpty()) {
                if (!anchorDragging && QLineF(anchorDragStart, event->position()).length() > 3.0) {
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
        if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) &&
            !selectedAnchorNodes.isEmpty()) {
            deleteSelectedAnchorNodes();
            return true;
        }
        return false;
    }

    void mousePress(QMouseEvent *event) {
        if (!clip || event->button() != Qt::LeftButton)
            return;
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
            if (note)
                clipController->onRemoveNotes({note->id()});
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
        if (editMode == DrawNote && !note) {
            interaction = Interaction::Draw;
            drawStart = snapLocalTick(localTickAt(event->position()));
            drawEnd = drawStart + TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
            drawKey = keyAt(event->position());
            scheduleSnapshot();
            return;
        }
        if (!note) {
            interaction = Interaction::RectSelect;
            rubberBandStart = event->position();
            rubberBandEnd = rubberBandStart;
            rubberBandBaseSelection = event->modifiers().testFlag(Qt::ControlModifier)
                                          ? appStatus->selectedNotes.get()
                                          : QList<int>();
            appStatus->selectedNotes = rubberBandBaseSelection;
            scheduleSnapshot();
            return;
        }

        auto selected = appStatus->selectedNotes.get();
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            if (selected.contains(note->id()))
                selected.removeAll(note->id());
            else
                selected.append(note->id());
        } else if (!selected.contains(note->id())) {
            selected = {note->id()};
        }
        appStatus->selectedNotes = selected;
        if (!selected.contains(note->id()))
            return;

        interactionNoteId = note->id();
        interactionStart = note->localStart();
        interactionLength = note->length();
        interactionKey = note->keyIndex();
        mouseDownTick = localTickAt(event->position());
        mouseDownKey = keyAt(event->position());
        const auto noteLeft = note->localStart() * pixelsPerTick() - cameraX;
        const auto noteRight = (note->localStart() + note->length()) * pixelsPerTick() - cameraX;
        constexpr double resizeTolerance = 6.0;
        if (std::abs(event->position().x() - noteLeft) <= resizeTolerance)
            interaction = Interaction::ResizeLeft;
        else if (std::abs(event->position().x() - noteRight) <= resizeTolerance)
            interaction = Interaction::ResizeRight;
        else
            interaction = Interaction::Move;
        scheduleSnapshot();
    }

    void mouseMove(QMouseEvent *event) {
        const auto key = keyAt(event->position());
        if (key != hoveredKey) {
            hoveredKey = key;
            emit q->keyHovered(key);
        }
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
        if (interaction == Interaction::Draw) {
            drawEnd = std::max(drawStart + 1, snapLocalTick(localTickAt(event->position())));
            scheduleSnapshot();
        } else if (interaction == Interaction::RectSelect) {
            rubberBandEnd = event->position();
            auto selected = rubberBandBaseSelection;
            const auto selection = QRectF(rubberBandStart, rubberBandEnd).normalized();
            for (const auto *note : clip->notes()) {
                const QRectF rect(note->localStart() * pixelsPerTick() - cameraX,
                                  (127 - note->keyIndex()) * noteHeight * scaleY - cameraY,
                                  note->length() * pixelsPerTick(), noteHeight * scaleY);
                if (selection.intersects(rect) && !selected.contains(note->id()))
                    selected.append(note->id());
            }
            appStatus->selectedNotes = selected;
            scheduleSnapshot();
        } else if (interaction != Interaction::None) {
            updateInteractionDelta(event->position(), event->modifiers());
            scheduleSnapshot();
        }
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
        if (interaction == Interaction::Draw) {
            const auto step = TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
            PianoRollGraphicsViewHelper::drawNote(drawStart, std::max(step, drawEnd - drawStart),
                                                  drawKey);
        } else if (interaction != Interaction::None && interaction != Interaction::RectSelect &&
                   interactionNoteId >= 0) {
            updateInteractionDelta(event->position(), event->modifiers());
            if (interaction == Interaction::Move) {
                if (interactionDeltaTick != 0 || interactionDeltaKey != 0)
                    clipController->onMoveNotes(appStatus->selectedNotes.get(),
                                                interactionDeltaTick, interactionDeltaKey);
            } else if (interaction == Interaction::ResizeLeft && interactionDeltaTick != 0) {
                clipController->onResizeNotesLeft({interactionNoteId}, interactionDeltaTick);
            } else if (interaction == Interaction::ResizeRight && interactionDeltaTick != 0) {
                clipController->onResizeNotesRight({interactionNoteId}, interactionDeltaTick);
            }
        }
        interaction = Interaction::None;
        interactionNoteId = -1;
        interactionDeltaTick = 0;
        interactionDeltaKey = 0;
        scheduleSnapshot();
    }

    void updateInteractionDelta(const QPointF &position, const Qt::KeyboardModifiers modifiers) {
        if (interaction == Interaction::None || interaction == Interaction::Draw)
            return;
        const auto rawDelta = localTickAt(position) - mouseDownTick;
        const auto target = modifiers.testFlag(Qt::AltModifier)
                                ? interactionStart + qRound(rawDelta)
                                : snapLocalTick(interactionStart + rawDelta);
        interactionDeltaTick = target - interactionStart;
        interactionDeltaKey = keyAt(position) - mouseDownKey;

        if (interaction == Interaction::Move) {
            int minimumKey = 127;
            int maximumKey = 0;
            bool found = false;
            for (const auto id : appStatus->selectedNotes.get()) {
                if (const auto *selectedNote = clip->findNoteById(id)) {
                    minimumKey = std::min(minimumKey, selectedNote->keyIndex());
                    maximumKey = std::max(maximumKey, selectedNote->keyIndex());
                    found = true;
                }
            }
            if (found)
                interactionDeltaKey =
                    std::clamp(interactionDeltaKey, -minimumKey, 127 - maximumKey);
        } else if (interaction == Interaction::ResizeLeft) {
            interactionDeltaTick = std::min(interactionDeltaTick, interactionLength - 1);
        } else if (interaction == Interaction::ResizeRight) {
            interactionDeltaTick = std::max(interactionDeltaTick, 1 - interactionLength);
        }
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
        vertices.clear();
        dpr = q->devicePixelRatioF();
        glyphAtlas.beginFrame();

        if (clip && q->width() > 0 && q->height() > 0) {
            const auto localStart = visibleLocalStartTick();
            const auto localEnd = visibleLocalEndTick();
            const auto sceneTop = cameraY;
            const auto sceneBottom = cameraY + q->height();

            appendBackground(localStart, localEnd, sceneTop, sceneBottom);
            appendTimeline(localStart, localEnd, sceneTop, sceneBottom);
            appendNotes(localStart, localEnd);
            appendPitch(localStart, localEnd);
            appendAnchors(localStart, localEnd);
            appendClipMask(localStart, localEnd, sceneTop, sceneBottom);
            appendPlaybackIndicators(sceneTop, sceneBottom);
            appendRubberBand();
            appendSplitPreview();
        }
        EditorRhiFrameData frame;
        frame.clearColor = q->whiteKeyColor();
        frame.physicalCameraOffset = QPointF(cameraX, cameraY) * dpr;
        frame.solidVertices = vertices;
        frame.textureBatches = glyphAtlas.textureBatches();
        q->submitFrame(std::move(frame));
    }

private:
    static double wheelDelta(const QWheelEvent *event, const bool preferHorizontal) {
        const auto angle = event->angleDelta();
        if (preferHorizontal && angle.x() != 0)
            return angle.x();
        if (angle.y() != 0)
            return angle.y();
        const auto pixel = event->pixelDelta();
        const auto value = preferHorizontal && pixel.x() != 0 ? pixel.x() : pixel.y();
        return value * 4.0;
    }

    double pixelsPerTick() const {
        return pixelsPerQuarterNote * scaleX / AppGlobal::ticksPerQuarterNote;
    }

    double sceneWidth() const {
        return clip ? clip->length() * pixelsPerTick() : q->width();
    }

    double sceneHeight() const {
        return 128.0 * noteHeight * scaleY;
    }

    double minimumScaleX() const {
        if (!clip || clip->length() <= 0 || q->width() <= 0)
            return 0.01;
        const auto baseWidth = clip->length() * pixelsPerQuarterNote /
                               static_cast<double>(AppGlobal::ticksPerQuarterNote);
        return std::min(5.0, q->width() / std::max(1.0, baseWidth));
    }

    double minimumScaleY() const {
        const auto fillScale = q->height() / (128.0 * noteHeight);
        return std::clamp(fillScale, 0.5, 8.0);
    }

    double visibleLocalStartTick() const {
        return cameraX / std::max(0.0001, pixelsPerTick());
    }

    double visibleLocalEndTick() const {
        return (cameraX + q->width()) / std::max(0.0001, pixelsPerTick());
    }

    void clampCamera() {
        cameraX = std::clamp(cameraX, 0.0, std::max(0.0, sceneWidth() - q->width()));
        cameraY = std::clamp(cameraY, 0.0, std::max(0.0, sceneHeight() - q->height()));
    }

    void viewportChanged(const bool scaleChanged) {
        clampCamera();
        q->notifyViewportChanged();
        Q_UNUSED(scaleChanged);
        scheduleSnapshot();
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
        const auto left = std::round(x * dpr);
        appendPhysicalRect(left, top * dpr, left + 1.0, bottom * dpr, color);
    }

    void appendPixelAlignedHorizontalLine(const double y, const double left, const double right,
                                          const QColor &color) {
        const auto top = std::round(y * dpr);
        appendPhysicalRect(left * dpr, top, right * dpr, top + 1.0, color);
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
            std::min(127, static_cast<int>(std::floor(127.0 - sceneTop / (noteHeight * scaleY))));
        const auto lastKey =
            std::max(0, static_cast<int>(std::floor(127.0 - sceneBottom / (noteHeight * scaleY))));
        for (int key = firstKey; key >= lastKey; --key) {
            const auto y = (127 - key) * noteHeight * scaleY;
            if (!PianoPaintUtils::isWhiteKey(key))
                appendLogicalRect(QRectF(left, y, right - left, noteHeight * scaleY), black);
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

    void appendNotes(const double localStart, const double localEnd) {
        const auto *palette = AppColorPalette::instance();
        const auto normalFill = palette->noteBackground(trackColorIndex);
        const auto normalBorder = palette->noteBorder(trackColorIndex);
        const auto selectedFill = palette->noteBackgroundSelected(trackColorIndex);
        const auto selectedBorder = q->noteSelectedBorderColor();
        const auto selectedNotes = appStatus->selectedNotes.get();
        for (const auto *note : clip->notes()) {
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
            const auto rect =
                QRectF(noteStart * pixelsPerTick(), (127 - noteKey) * noteHeight * scaleY,
                       noteLength * pixelsPerTick(), noteHeight * scaleY);
            appendLogicalRect(rect, selected ? selectedBorder : normalBorder);
            const auto inset = kNoteBorderWidth;
            appendLogicalRect(rect.adjusted(inset, inset, -inset, -inset),
                              selected ? selectedFill : normalFill);

            if (scaleX >= 0.3 && rect.width() > 8.0 && rect.height() > 8.0) {
                QFont font = q->font();
                font.setPixelSize(std::max(1, qRound(q->noteFontPixelSize() * dpr)));
                const auto foreground = palette->noteForeground(trackColorIndex);
                const QRectF scaledClip((rect.left() + 3.0) * dpr, rect.top() * dpr,
                                        std::max(0.0, rect.width() - 6.0) * dpr,
                                        rect.height() * dpr);
                const auto editingLyric =
                    inlineEditField == InlineEditField::Lyric && inlineEditingNoteId == note->id();
                if (!editingLyric) {
                    glyphAtlas.appendText(
                        note->lyric(), font,
                        QPointF((rect.left() + 3.0) * dpr, (rect.top() + 1.0) * dpr), foreground,
                        scaledClip);
                }

                const auto pronunciation = note->pronunciation();
                const auto pronunciationText = pronunciation.result();
                const auto editingPronunciation =
                    inlineEditField == InlineEditField::Pronunciation &&
                    inlineEditingNoteId == note->id();
                if (!pronunciationText.isEmpty() && !editingPronunciation) {
                    const auto pronunciationColor = pronunciation.isEdited()
                                                        ? palette->phonemeEdited(trackColorIndex)
                                                        : q->pronunciationTextColor();
                    const QRectF pronunciationClip(rect.left() * dpr, rect.bottom() * dpr,
                                                   rect.width() * dpr,
                                                   q->noteFontPixelSize() * 1.5 * dpr);
                    glyphAtlas.appendText(
                        pronunciationText, font,
                        QPointF((rect.left() + 3.0) * dpr, (rect.bottom() + 1.0) * dpr),
                        pronunciationColor, pronunciationClip);
                }
            }
        }
        if (interaction == Interaction::Draw && drawEnd > drawStart) {
            const QRectF rect(drawStart * pixelsPerTick(), (127 - drawKey) * noteHeight * scaleY,
                              (drawEnd - drawStart) * pixelsPerTick(), noteHeight * scaleY);
            appendLogicalRect(rect, selectedBorder);
            const auto inset = kNoteBorderWidth;
            appendLogicalRect(rect.adjusted(inset, inset, -inset, -inset), selectedFill);
        }
    }

    void appendPitch(const double localStart, const double localEnd) {
        const auto *pitch = clip->params.getParamByName(ParamInfo::Pitch);
        if (!pitch)
            return;
        appendPitchCurves(pitch->curves(Param::Original), localStart, localEnd,
                          q->paramOriginalCurveColor());
        auto editedColor = q->paramEditedCurveColor();
        editedColor.setAlpha(std::min(editedColor.alpha(), 180));
        if (pitchEditing)
            appendPitchCurves(pitchPreviewCurves, localStart, localEnd, editedColor);
        else
            appendPitchCurves(pitch->curves(Param::Edited), localStart, localEnd, editedColor);
    }

    void appendPitchCurves(const QList<Curve *> &curves, const double localStart,
                           const double localEnd, const QColor &color) {
        for (const auto *curve : curves) {
            if (!curve || curve->type() != Curve::Draw || curve->localEndTick() < localStart)
                continue;
            if (curve->localStart() > localEnd)
                break;
            appendPitchCurve(*static_cast<const DrawCurve *>(curve), localStart, localEnd, color);
        }
    }

    void appendPitchCurves(const QList<DrawCurve *> &curves, const double localStart,
                           const double localEnd, const QColor &color) {
        for (const auto *curve : curves) {
            if (!curve || curve->localEndTick() < localStart)
                continue;
            if (curve->localStart() > localEnd)
                break;
            appendPitchCurve(*curve, localStart, localEnd, color);
        }
    }

    void appendPitchCurve(const DrawCurve &curve, const double localStart, const double localEnd,
                          const QColor &color) {
        const auto &values = curve.values();
        if (values.size() < 2)
            return;
        const auto startIndex =
            std::max(0, static_cast<int>(std::floor((localStart - curve.localStart()) /
                                                    static_cast<double>(curve.step))) -
                            1);
        QVector<QPointF> points;
        points.reserve(values.size() - startIndex);
        double lastAcceptedX = -1e30;
        for (int i = startIndex; i < values.size(); ++i) {
            const auto tick = curve.localStart() + i * curve.step;
            if (tick > localEnd + curve.step)
                break;
            const auto x = tick * pixelsPerTick();
            const auto value = MathUtils::clip(values.at(i), 0, 12700);
            const auto y = (12700 - value + 50) * scaleY * noteHeight / 100.0;
            if (points.isEmpty() || (x - lastAcceptedX) * dpr >= 1.0) {
                points.append(QPointF(x, y) * dpr);
                lastAcceptedX = x;
            }
        }
        QVector<EditorRhiSolidVertex> stroke;
        EditorRhiGeometry::appendAntialiasedStroke(stroke, points, kPitchLineWidth * dpr, color,
                                                   1.0, 3.0);
        vertices.reserve(vertices.size() + stroke.size());
        for (const auto &vertex : stroke)
            vertices.append(
                {vertex.x, vertex.y, vertex.r, vertex.g, vertex.b, vertex.a, vertex.coverage});
    }

    QPointF anchorNodeScenePosition(const AnchorNode *node) const {
        return {node->pos() * pixelsPerTick(),
                (12700 - node->value() + 50) * scaleY * noteHeight / 100.0};
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
                const auto sceneY = (12700.0 - value + 50.0) * scaleY * noteHeight / 100.0;
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
                QPointF(endSceneX, (12700.0 - value + 50.0) * scaleY * noteHeight / 100.0) * dpr;
            if (points.isEmpty() || QLineF(points.constLast(), endpoint).length() > 0.01)
                points.append(endpoint);
        }

        if (points.size() < 2)
            return;
        QVector<EditorRhiSolidVertex> stroke;
        if (!dashed) {
            EditorRhiGeometry::appendAntialiasedStroke(stroke, points, kPitchLineWidth * dpr, color,
                                                       1.0, 3.0);
        } else {
            constexpr double dashLength = 5.0;
            constexpr double gapLength = 3.0;
            bool drawing = true;
            double remaining = dashLength * dpr;
            QVector<QPointF> dash;
            dash.append(points.first());
            for (int i = 0; i + 1 < points.size(); ++i) {
                auto cursor = points.at(i);
                const auto end = points.at(i + 1);
                auto segment = end - cursor;
                auto segmentLength = std::hypot(segment.x(), segment.y());
                while (segmentLength > 0.001) {
                    const auto advance = std::min(remaining, segmentLength);
                    const auto next = cursor + segment * (advance / segmentLength);
                    if (drawing) {
                        if (dash.isEmpty())
                            dash.append(cursor);
                        dash.append(next);
                    }
                    cursor = next;
                    segment = end - cursor;
                    segmentLength = std::hypot(segment.x(), segment.y());
                    remaining -= advance;
                    if (remaining <= 0.001) {
                        if (drawing && dash.size() >= 2)
                            EditorRhiGeometry::appendAntialiasedStroke(
                                stroke, dash, kPitchLineWidth * dpr, color, 1.0, 3.0);
                        dash.clear();
                        drawing = !drawing;
                        remaining = (drawing ? dashLength : gapLength) * dpr;
                    }
                }
            }
            if (drawing && dash.size() >= 2)
                EditorRhiGeometry::appendAntialiasedStroke(stroke, dash, kPitchLineWidth * dpr,
                                                           color, 1.0, 3.0);
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

    void appendAnchorCurve(const AnchorCurve *curve, const double localStart, const double localEnd,
                           const QColor &curveColor, const QColor &nodeColor,
                           const bool drawNodes) {
        if (!curve)
            return;
        auto nodes = curve->nodes().toList();
        if (anchorDragging && !anchorDragInfos.isEmpty()) {
            for (const auto &info : anchorDragInfos) {
                if (info.sourceCurve == curve && info.targetCurve)
                    nodes.removeOne(info.node);
            }
        }
        appendAnchorStroke(nodes, localStart, localEnd, curveColor);
        if (!drawNodes)
            return;
        for (auto *node : nodes) {
            const auto selected = selectedAnchorNodes.contains(node);
            const auto hovered = node == hoveredAnchorNode;
            appendAnchorNode(node, selected ? q->anchorSelectedColor() : nodeColor,
                             selected || hovered);
        }
    }

    void appendAnchorPreview(const double localStart, const double localEnd) {
        if (!anchorEditing || anchorDragging || !anchorCursorInView)
            return;

        auto previewColor = q->anchorPreviewColor();
        previewColor.setAlpha(160);
        if (showAnchorMergePreview && currentAnchorCurve && anchorMergeCandidateCurve) {
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

        auto nodes = anchorPreviewCurve->nodes().toList();
        const auto point = anchorPointAt(anchorPreviewPosition);
        AnchorNode virtualNode(point.x(), point.y());
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
        previewColor.setAlpha(160);
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
        const auto viewportRect = anchorSelectionRect.normalized();
        const auto sceneRect = viewportRect.translated(cameraX, cameraY);
        auto fill = q->anchorPreviewColor();
        fill.setAlpha(64);
        auto border = q->anchorPreviewColor();
        border.setAlpha(200);
        appendLogicalRect(sceneRect, fill);
        appendLine(sceneRect.topLeft(), sceneRect.topRight(), 1.5, border);
        appendLine(sceneRect.topRight(), sceneRect.bottomRight(), 1.5, border);
        appendLine(sceneRect.bottomRight(), sceneRect.bottomLeft(), 1.5, border);
        appendLine(sceneRect.bottomLeft(), sceneRect.topLeft(), 1.5, border);
    }

    void appendAnchors(const double localStart, const double localEnd) {
        const auto active = editMode == EditPitchAnchor;
        auto nodeColor = q->anchorColor();
        nodeColor.setAlpha(active ? 255 : 80);
        auto curveColor = q->anchorCurveColor();
        curveColor.setAlpha(active ? 200 : 60);
        if (active) {
            appendAnchorPreview(localStart, localEnd);
            appendAnchorDragPreview(localStart, localEnd);
        }
        for (const auto *curve : anchorCurves)
            appendAnchorCurve(curve, localStart, localEnd, curveColor, nodeColor, active);
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

    void appendPlaybackIndicators(const double sceneTop, const double sceneBottom) {
        const auto lastX = (lastPlaybackPosition - clip->start()) * pixelsPerTick();
        const auto currentX = (playbackPosition - clip->start()) * pixelsPerTick();
        appendPixelAlignedVerticalLine(lastX, sceneTop, sceneBottom, QColor(150, 150, 150));
        appendPixelAlignedVerticalLine(currentX, sceneTop, sceneBottom, QColor(220, 220, 220));
    }

    void appendRubberBand() {
        if (interaction != Interaction::RectSelect)
            return;
        const auto viewportRect = QRectF(rubberBandStart, rubberBandEnd).normalized();
        const auto sceneRect = viewportRect.translated(cameraX, cameraY);
        appendLogicalRect(sceneRect, QColor(155, 186, 255, 48));
        constexpr double borderWidth = 1.0;
        const auto border = QColor(155, 186, 255, 190);
        appendLogicalRect(QRectF(sceneRect.left(), sceneRect.top(), sceneRect.width(), borderWidth),
                          border);
        appendLogicalRect(QRectF(sceneRect.left(), sceneRect.bottom() - borderWidth,
                                 sceneRect.width(), borderWidth),
                          border);
        appendLogicalRect(
            QRectF(sceneRect.left(), sceneRect.top(), borderWidth, sceneRect.height()), border);
        appendLogicalRect(QRectF(sceneRect.right() - borderWidth, sceneRect.top(), borderWidth,
                                 sceneRect.height()),
                          border);
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
        const auto noteTop = (127 - note->keyIndex()) * noteHeight * scaleY;
        const auto lineTop = noteTop - extensionLength;
        const auto lineBottom = noteTop + noteHeight * scaleY + extensionLength;
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
    bool snapshotScheduled = false;
    bool fallbackRequested = false;
    int trackColorIndex = 0;
    PianoRollEditMode editMode = Select;
    enum class Interaction { None, Move, ResizeLeft, ResizeRight, Draw, RectSelect };
    Interaction interaction = Interaction::None;
    int interactionNoteId = -1;
    int interactionStart = 0;
    int interactionLength = 0;
    int interactionKey = 60;
    int interactionDeltaTick = 0;
    int interactionDeltaKey = 0;
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
    QList<AnchorDragInfo> anchorDragInfos;
    QRectF anchorSelectionRect;
    QPointF anchorPreviewPosition;
    QPointF anchorDragStart;
    AnchorCurve *anchorPreviewCurve = nullptr;
    AnchorCurve *anchorMergeCandidateCurve = nullptr;
    AnchorNode *anchorMergeEndpointNode = nullptr;
    bool showAnchorPreview = false;
    bool showAnchorMergePreview = false;
    double scaleX = 1.0;
    double scaleY = 1.0;
    double cameraX = 0.0;
    double cameraY = 0.0;
    double playbackPosition = 0.0;
    double lastPlaybackPosition = 0.0;
    double dpr = 1.0;
    QVector<Vertex> vertices;
    TimelineLineEmitter timelineEmitter;
    EditorGlyphAtlas glyphAtlas;

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
    InlineTextEditOverlay *inlineEditor = nullptr;
    InlineEditField inlineEditField = InlineEditField::None;
    int inlineEditingNoteId = -1;
};

PianoRollRhiWidget::PianoRollRhiWidget(QWidget *parent)
    : EditorRhiWidget(QStringLiteral("PianoRollRhi"), parent), d(std::make_unique<Private>(this)) {
    setObjectName(QStringLiteral("PianoRollRhiWidget"));
    setMouseTracking(true);
    d->initializeInlineEditor();
    connect(this, &EditorRhiWidget::backendFailed, this,
            [this](const QString &) { d->requestFallback(); });
    connect(appStatus, &AppStatus::pianoRollQuantizeChanged, this,
            [this] { d->scheduleSnapshot(); });
    connect(appStatus, &AppStatus::noteSelectionChanged, this, [this] { d->scheduleSnapshot(); });
    connect(appModel, &AppModel::timelineChanged, this, [this] { d->scheduleSnapshot(); });
}

PianoRollRhiWidget::~PianoRollRhiWidget() {
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
    return d->scaleX;
}

double PianoRollRhiWidget::scaleY() const {
    return d->scaleY;
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

bool PianoRollRhiWidget::revealFocus(const HistoryFocus &focus, bool) {
    return d->revealFocus(focus);
}

void PianoRollRhiWidget::setEditMode(const PianoRollEditMode mode) {
    if (d->editMode != mode) {
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

void PianoRollRhiWidget::setPlaybackPosition(const double tick) {
    d->playbackPosition = tick;
    d->scheduleSnapshot();
}

void PianoRollRhiWidget::setLastPlaybackPosition(const double tick) {
    d->lastPlaybackPosition = tick;
    d->scheduleSnapshot();
}

void PianoRollRhiWidget::resizeEvent(QResizeEvent *event) {
    EditorRhiWidget::resizeEvent(event);
    d->resize();
}

void PianoRollRhiWidget::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() == Qt::ControlModifier)
        onWheelHorScale(event);
    else if (event->modifiers() == Qt::AltModifier)
        onWheelVerScale(event);
    else if (event->modifiers() == Qt::ShiftModifier)
        onWheelHorScroll(event);
    else if (event->modifiers() == Qt::NoModifier)
        onWheelVerScroll(event);
    event->accept();
}

void PianoRollRhiWidget::mousePressEvent(QMouseEvent *event) {
    setFocus(Qt::MouseFocusReason);
    d->mousePress(event);
    event->accept();
}

void PianoRollRhiWidget::mouseMoveEvent(QMouseEvent *event) {
    d->mouseMove(event);
    event->accept();
}

void PianoRollRhiWidget::mouseReleaseEvent(QMouseEvent *event) {
    d->mouseRelease(event);
    event->accept();
}

void PianoRollRhiWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if (d->clip && d->editMode == EditPitchAnchor && event->button() == Qt::LeftButton) {
        setFocus(Qt::MouseFocusReason);
        d->createAnchorAt(event->position());
        d->anchorDragStart = event->position();
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
    }
    EditorRhiWidget::mouseDoubleClickEvent(event);
}

void PianoRollRhiWidget::keyPressEvent(QKeyEvent *event) {
    if (d->editMode == EditPitchAnchor && d->keyPressAnchor(event)) {
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        if (d->pitchEditing) {
            d->cancelPitchEdit();
            event->accept();
            return;
        }
        d->interaction = Private::Interaction::None;
        d->interactionNoteId = -1;
        d->interactionDeltaTick = 0;
        d->interactionDeltaKey = 0;
        d->scheduleSnapshot();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        clipController->onDeleteSelectedNotes();
        event->accept();
        return;
    }
    EditorRhiWidget::keyPressEvent(event);
}

void PianoRollRhiWidget::leaveEvent(QEvent *event) {
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

        Menu menu(this);
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

        auto *linear = menu.addAction(tr("Linear"));
        linear->setCheckable(true);
        linear->setChecked(allSame && currentMode == AnchorNode::Linear);
        linear->setEnabled(!allAreLast);
        connect(linear, &QAction::triggered, this, [this, isLastNode] {
            for (auto *selected : d->selectedAnchorNodes) {
                if (!isLastNode(selected))
                    selected->setInterpMode(AnchorNode::Linear);
            }
            d->commitAnchorEdit();
        });

        auto *hermite = menu.addAction(tr("Hermite"));
        hermite->setCheckable(true);
        hermite->setChecked(allSame && currentMode == AnchorNode::Hermite);
        hermite->setEnabled(!allAreLast);
        connect(hermite, &QAction::triggered, this, [this, isLastNode] {
            for (auto *selected : d->selectedAnchorNodes) {
                if (!isLastNode(selected))
                    selected->setInterpMode(AnchorNode::Hermite);
            }
            d->commitAnchorEdit();
        });

        auto *interpolationGroup = new QActionGroup(&menu);
        interpolationGroup->setExclusive(true);
        interpolationGroup->addAction(linear);
        interpolationGroup->addAction(hermite);
        if (!allSame) {
            linear->setChecked(false);
            hermite->setChecked(false);
        }

        menu.addSeparator();
        auto *remove = menu.addAction(tr("Delete"));
        connect(remove, &QAction::triggered, this, [this] { d->deleteSelectedAnchorNodes(); });
        menu.exec(event->globalPos());
        event->accept();
        return;
    }

    Menu menu(this);
    if (auto *note = d->noteAt(event->pos())) {
        if (!appStatus->selectedNotes.get().contains(note->id()))
            appStatus->selectedNotes = QList<int>{note->id()};
        auto *cut = menu.addAction(tr("Cut"));
        connect(cut, &QAction::triggered, clipController,
                &ClipController::cutSelectedNotesWithParams);
        auto *copy = menu.addAction(tr("Copy"));
        connect(copy, &QAction::triggered, clipController,
                &ClipController::copySelectedNotesWithParams);
        auto *split = menu.addAction(tr("Split Note"));
        const auto splitTick = qRound(d->localTickAt(event->pos())) + d->clip->start();
        connect(split, &QAction::triggered, this, [noteId = note->id(), splitTick] {
            PianoRollGraphicsViewHelper::splitNote(noteId, splitTick);
        });
        auto *remove = menu.addAction(tr("Delete"));
        connect(remove, &QAction::triggered, clipController,
                &ClipController::onDeleteSelectedNotes);
    } else {
        const auto *mimeData = QGuiApplication::clipboard()->mimeData();
        const auto format = ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams);
        auto *paste = menu.addAction(tr("Paste"));
        paste->setEnabled(mimeData && mimeData->hasFormat(format));
        if (paste->isEnabled()) {
            const auto info = NotesParamsInfo::deserializeFromJson(
                QJsonDocument::fromJson(mimeData->data(format)).object());
            const auto tick = qRound(d->localTickAt(event->pos())) + d->clip->start();
            connect(paste, &QAction::triggered, this,
                    [info, tick] { clipController->pasteNotesWithParams(info, tick); });
        }
    }
    menu.exec(event->globalPos());
}

void PianoRollRhiWidget::onRhiReady() {
    d->scheduleSnapshot();
}

void PianoRollRhiWidget::onDevicePixelRatioChanged() {
    d->glyphAtlas.clear();
    d->scheduleSnapshot();
}

void PianoRollRhiWidget::notifyViewportChanged() {
    emit timeRangeChanged(startTick(), endTick());
    emit keyRangeChanged(topKeyIndex(), bottomKeyIndex());
    emit scaleChanged(scaleX(), scaleY());
    emit horizontalBarValueChanged(horizontalBarValue());
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
