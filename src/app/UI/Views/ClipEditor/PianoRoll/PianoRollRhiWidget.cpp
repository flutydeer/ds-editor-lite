#include "PianoRollRhiWidget.h"

#include "NoteView.h"
#include "PianoPaintUtils.h"
#include "PianoRollGraphicsViewHelper.h"
#include "PronunciationView.h"
#include "Controller/ClipController.h"
#include "Global/AppGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Utils/AppColorPalette.h"
#include "UI/Utils/ITimelinePainter.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/Common/EditorRhiGeometry.h"
#include "UI/Views/Common/EditorGlyphAtlas.h"
#include "Global/ControllerGlobal.h"

#include <lite/GUI/Controls/Menu.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/Support/MathUtils.h>
#include <lite/MusicBase/TimelineSnapUtils.h>

#include <QEvent>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QMimeData>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <functional>

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
    explicit Private(PianoRollRhiWidget *q) : q(q) {
    }

    void setDataContext(SingingClip *newClip) {
        if (clip)
            QObject::disconnect(clip, nullptr, q, nullptr);

        clip = newClip;
        cameraInitialized = false;
        if (clip) {
            QObject::connect(clip, &SingingClip::noteChanged, q, [this] { scheduleSnapshot(); });
            QObject::connect(clip, &SingingClip::paramChanged, q,
                             [this](const ParamInfo::Name name, Param::Type) {
                                 if (name == ParamInfo::Pitch)
                                     scheduleSnapshot();
                             });
            QObject::connect(clip, &SingingClip::propertyChanged, q,
                             [this] { scheduleSnapshot(); });
        }
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

    int snapLocalTick(const double localTick) const {
        const auto step = TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
        const auto globalTick = qRound(localTick) + (clip ? clip->start() : 0);
        return TimelineSnapUtils::snapNearest(globalTick, step, appModel->timeline()) -
               (clip ? clip->start() : 0);
    }

    void mousePress(QMouseEvent *event) {
        if (!clip || event->button() != Qt::LeftButton)
            return;
        auto *note = noteAt(event->position());
        if (editMode == EraseNote) {
            if (note)
                clipController->onRemoveNotes({note->id()});
            return;
        }
        if (editMode == SplitNote) {
            if (note)
                PianoRollGraphicsViewHelper::splitNote(
                    note->id(), qRound(localTickAt(event->position())) + clip->start());
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
            appendClipMask(localStart, localEnd, sceneTop, sceneBottom);
            appendPlaybackIndicators(sceneTop, sceneBottom);
            appendRubberBand();
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
                glyphAtlas.appendText(note->lyric(), font,
                                      QPointF((rect.left() + 3.0) * dpr, (rect.top() + 1.0) * dpr),
                                      foreground, scaledClip);

                const auto pronunciation = note->pronunciation();
                const auto pronunciationText = pronunciation.result();
                if (!pronunciationText.isEmpty()) {
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
        appendPitchCurves(pitch->curves(Param::Edited), localStart, localEnd, editedColor);
    }

    void appendPitchCurves(const QList<Curve *> &curves, const double localStart,
                           const double localEnd, const QColor &color) {
        for (const auto *curve : curves) {
            if (!curve || curve->type() != Curve::Draw || curve->localEndTick() < localStart)
                continue;
            if (curve->localStart() > localEnd)
                break;
            const auto *drawCurve = static_cast<const DrawCurve *>(curve);
            const auto &values = drawCurve->values();
            if (values.size() < 2)
                continue;
            const auto startIndex =
                std::max(0, static_cast<int>(std::floor((localStart - drawCurve->localStart()) /
                                                        static_cast<double>(drawCurve->step))) -
                                1);
            QVector<QPointF> points;
            points.reserve(values.size() - startIndex);
            double lastAcceptedX = -1e30;
            for (int i = startIndex; i < values.size(); ++i) {
                const auto tick = drawCurve->localStart() + i * drawCurve->step;
                if (tick > localEnd + drawCurve->step)
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
    QPointF rubberBandStart;
    QPointF rubberBandEnd;
    QList<int> rubberBandBaseSelection;
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
    QColor barLineColor{86, 90, 98};
    QColor beatLineColor{62, 66, 73};
    QColor commonLineColor{47, 50, 56};
};

PianoRollRhiWidget::PianoRollRhiWidget(QWidget *parent)
    : EditorRhiWidget(QStringLiteral("PianoRollRhi"), parent), d(std::make_unique<Private>(this)) {
    setObjectName(QStringLiteral("PianoRollRhiWidget"));
    setMouseTracking(true);
    connect(this, &EditorRhiWidget::backendFailed, this,
            [this](const QString &) { d->requestFallback(); });
    connect(appStatus, &AppStatus::pianoRollQuantizeChanged, this,
            [this] { d->scheduleSnapshot(); });
    connect(appStatus, &AppStatus::noteSelectionChanged, this, [this] { d->scheduleSnapshot(); });
    connect(appModel, &AppModel::timelineChanged, this, [this] { d->scheduleSnapshot(); });
}

PianoRollRhiWidget::~PianoRollRhiWidget() = default;

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
    d->editMode = mode;
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

void PianoRollRhiWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
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
    EditorRhiWidget::leaveEvent(event);
}

void PianoRollRhiWidget::contextMenuEvent(QContextMenuEvent *event) {
    if (!d->clip)
        return;

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
