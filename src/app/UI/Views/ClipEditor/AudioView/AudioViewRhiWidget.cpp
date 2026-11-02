#include "AudioViewRhiWidget.h"

#include "AudioLaneGeometry.h"
#include "Global/AppGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Utils/AppColorPalette.h"
#include "UI/Utils/WaveformRhiUtils.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/Common/AudioLoadHint.h"
#include "UI/Views/Common/EditorGlyphAtlas.h"
#include "UI/Views/Common/EditorRhiGeometry.h"
#include "UI/Views/Common/EditorRhiScrollBarController.h"
#include "UI/Views/Common/EditorTouchController.h"
#include "UI/Views/Common/EditorViewportController.h"
#include "UI/Views/Common/EditorWheelController.h"
#include "UI/Views/Common/TimelineLineEmitter.h"
#include "UI/Views/TrackEditor/AudioWaveformSampler.h"

#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QCoreApplication>
#include <QFontMetricsF>
#include <QHideEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>

#include <cmath>
#include <memory>
#include <vector>

using namespace ClipEditorGlobal;

class AudioViewRhiWidget::Private {
public:
    explicit Private(AudioViewRhiWidget *q) : q(q), viewport(nullptr), wheel(&viewport, q, q) {
        viewport.setPixelsPerQuarterNote(pixelsPerQuarterNote);
        viewport.setScaleBounds(0.01, 5.0, 1.0, 1.0);
        // The lanes tile the canvas, so the horizontal axis fills the viewport
        // while the vertical axis has neither content nor a scroll range
        viewport.setEnsureContentFillsViewport(true, false);
        viewport.setVerticalContent(0.0, 1.0);
        QObject::connect(&viewport, &EditorViewportController::viewportChanged, q, [this] {
            updateScrollBars();
            this->q->notifyViewportChanged();
            scheduleSnapshot();
        });
        QObject::connect(ThemeManager::instance(), &ThemeManager::themeChanged, q,
                         [this] { scheduleSnapshot(); });
        hintTicker.setInterval(16);
        QObject::connect(&hintTicker, &QTimer::timeout, q, [this] { scheduleSnapshot(); });
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
        const auto contentSize =
            content.isValid() ? QSizeF(viewport.tickToSceneX(content.materialEndTick()), q->height())
                              : QSizeF(q->size());
        scrollBars->setMetrics(contentSize, viewport.offset(),
                               QSizeF(std::max(1, q->width() / 10), std::max(1, q->height() / 10)));
    }

    [[nodiscard]] bool hasContent() const {
        return content.isValid() && q->width() > 0 && q->height() > 0;
    }

    void setContent(const AudioViewContent &newContent) {
        wheel.stop();
        viewport.stopAnimation();
        if (content.clipId != newContent.clipId)
            viewportPositionPending = true;
        content = newContent;
        rebuildSamplers();
        // The content range starts at the clip's first audible tick rather than at the
        // timeline origin, so the scroll range covers the material and nothing else.
        // The viewport keeps reporting global ticks, which is what the ruler, the
        // playhead and the peak caches work in
        viewport.setContentTickRange(
            content.contentStartTick(),
            std::max(content.contentStartTick(), content.materialEndTick()));
        updateScrollBars();
        // The ring only has to turn while the clip still has nothing to draw
        const auto loading =
            content.isValid() && content.loadState == AppGlobal::AudioClipLoadState::Loading;
        if (loading && !hintTicker.isActive())
            hintTicker.start();
        else if (!loading && hintTicker.isActive())
            hintTicker.stop();
        scheduleSnapshot();
    }

    void rebuildSamplers() {
        const auto lanes = static_cast<size_t>(content.isValid() ? content.laneCount() : 1);
        while (samplers.size() > lanes)
            samplers.pop_back();
        while (samplers.size() < lanes)
            samplers.push_back(std::make_unique<AudioWaveformSampler>());
        for (auto &sampler : samplers) {
            sampler->setPath(content.path);
            sampler->invalidate();
        }
    }

    void positionViewportAtContent() {
        if (!content.isValid() || !viewportPositionPending || q->width() <= 0)
            return;
        viewportPositionPending = false;
        // The clip is the whole content, so the first frame starts at its left end. Centring
        // would land in the middle of the material, which is not where the clip begins
        viewport.setOffset({0.0, viewport.verticalOffset()});
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
        frame.textureBatches.clear();
        dpr = q->devicePixelRatioF();
        glyphAtlas.beginFrame();

        if (hasContent()) {
            appendBackground();
            appendTimeline();
            appendWaveforms();
            appendLoadHint();
            appendClipMask();
            appendLastPlaybackIndicator();
        }

        frame.clearColor = content.isValid() ? laneBackgroundColor : emptyViewBackgroundColor;
        frame.physicalCameraOffset = viewport.offset() * dpr;
        drawList.finish(vertices.size());
        glyphAtlas.populateTextureBatches(frame.textureBatches);
        frame.solidVertices = std::move(vertices);
        frame.drawList = std::move(drawList);
        q->submitFrame(std::move(frame));
        updatePlaybackOverlay();
    }

    void updatePlaybackOverlay() {
        playbackOverlayRects.clear();
        if (hasContent()) {
            const auto currentX = viewport.tickToSceneX(playbackPosition) * dpr;
            EditorRhiGeometry::appendAntialiasedVerticalOverlay(
                playbackOverlayRects, currentX - viewport.horizontalOffset() * dpr, 0.0,
                q->height() * dpr, dpr, playPosIndicatorColor);
        }
        playbackOverlayRects = q->submitOverlay(std::move(playbackOverlayRects));
    }

    [[nodiscard]] QRectF laneSceneRect(const int lane) const {
        return AudioLaneGeometry::laneRect(viewport.visibleSceneRect(), content.channels(), lane);
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

    void appendSceneRect(const QRectF &rect, const QColor &color) {
        appendPhysicalRect(rect.left() * dpr, rect.top() * dpr, rect.right() * dpr,
                           rect.bottom() * dpr, color);
    }

    void appendPixelAlignedVerticalLine(const double x, const double top, const double bottom,
                                        const QColor &color, const double logicalWidth = 1.0) {
        EditorRhiGeometry::appendAntialiasedVerticalLine(vertices, x * dpr, top * dpr, bottom * dpr,
                                                         logicalWidth * dpr, color,
                                                         viewport.horizontalOffset() * dpr);
    }

    void appendHorizontalLine(const double y, const double left, const double right,
                              const QColor &color) {
        EditorRhiGeometry::appendAntialiasedHorizontalLine(vertices, y * dpr, left * dpr,
                                                           right * dpr, dpr, color,
                                                           viewport.verticalOffset() * dpr);
    }

    void appendBackground() {
        const auto lanes = content.laneCount();
        const auto visible = viewport.visibleSceneRect();
        for (auto lane = 0; lane < lanes; lane++) {
            const auto band = laneSceneRect(lane);
            if (band.isEmpty())
                continue;
            appendSceneRect(band, laneBackgroundColor);
            if (lane > 0)
                appendHorizontalLine(qRound(band.top()) + 0.5, visible.left(), visible.right(),
                                     laneSeparatorColor);
        }
    }

    void appendTimeline() {
        const auto visible = viewport.visibleSceneRect();
        const auto width = std::max(1.0, visible.width());
        const auto top = visible.top();
        const auto bottom = visible.bottom();
        timelineEmitter.emitLines(
            appModel->timeline(), appStatus->pianoRollQuantize, viewport.startTick(),
            viewport.endTick(), width, barLineColor, beatLineColor, commonLineColor,
            [this, top, bottom](const int tick, const QColor &color) {
                appendPixelAlignedVerticalLine(viewport.tickToSceneX(tick), top, bottom, color);
            });
    }

    void appendWaveforms() {
        const auto visible = viewport.visibleSceneRect();
        const auto lanes = content.laneCount();
        const auto color = AppColorPalette::instance()->baseColor(content.trackColorIndex);
        const auto loaded = content.loadState == AppGlobal::AudioClipLoadState::Loaded;
        for (auto lane = 0; lane < lanes; lane++) {
            const auto band = laneSceneRect(lane);
            if (band.isEmpty())
                continue;
            // The silence line stays visible whatever the waveform does
            appendHorizontalLine(qRound(band.center().y()) + 0.5, band.left(), band.right(),
                                 waveformCenterLineColor);
            if (!loaded)
                continue;

            const auto &sampler = samplers.at(static_cast<size_t>(lane));
            const auto waveform = sampler->sample({
                .audioInfo = &content.audioInfo,
                .timeline = &content.timeline,
                .materialStartTick = content.start,
                .visibleStartTick = content.contentStartTick(),
                .channel = lane,
                .previewSceneRect = band,
                .visibleSceneRect = visible,
                .horizontalScale = viewport.horizontalScale(),
                .pixelsPerQuarterNote = pixelsPerQuarterNote,
                .leftMarginPx = 0.0,
                .sceneTickOffset = content.contentStartTick(),
                .devicePixelRatio = dpr,
            });
            WaveformRhiUtils::appendWaveform(
                vertices, waveform,
                QRectF(band.left() * dpr, band.top() * dpr, band.width() * dpr, band.height() * dpr),
                color, dpr, viewport.offset() * dpr);
        }
    }

    // Same hint as the track editor and the QGraphics backend: a turning ring plus the text while
    // the material decodes, only the text when the file is gone
    void appendLoadHint() {
        const auto text = AudioLoadHint::textFor(content.loadState, content.errorMessage);
        if (text.isEmpty())
            return;
        const auto animated = content.loadState == AppGlobal::AudioClipLoadState::Loading;
        auto color = AudioLoadHint::colorFor(
            content.loadState, AppColorPalette::instance()->baseColor(content.trackColorIndex));

        // Centred in the visible canvas rather than in the whole material: the lanes tile the
        // viewport, so a clip scrolled far from its own centre still shows the hint
        auto labelRect = viewport.visibleSceneRect();
        labelRect = QRectF(labelRect.left() * dpr, labelRect.top() * dpr, labelRect.width() * dpr,
                           labelRect.height() * dpr);
        if (labelRect.isEmpty())
            return;

        const QFont font = q->font();
        const QFontMetricsF metrics(font);
        const auto textSize = QSizeF(std::ceil(metrics.horizontalAdvance(text) * dpr),
                                     std::ceil(metrics.height() * dpr));
        auto topLeft =
            labelRect.center() - QPointF(textSize.width() * 0.5, textSize.height() * 0.5);
        if (animated) {
            const auto block = AudioLoadHint::layout(labelRect, textSize.width(), dpr);
            auto trackColor = color;
            trackColor.setAlphaF(trackColor.alphaF() * AudioLoadHint::trackAlpha);
            QVector<EditorRhiSolidVertex> ringVertices;
            EditorRhiGeometry::appendAntialiasedStroke(
                ringVertices, AudioLoadHint::trackCenterline(block.ringCenter, block.ringRadius),
                AudioLoadHint::ringStrokeWidth * dpr, trackColor);
            EditorRhiGeometry::appendAntialiasedStroke(
                ringVertices,
                AudioLoadHint::arcCenterline(block.ringCenter, block.ringRadius,
                                             AudioLoadHint::clockMs()),
                AudioLoadHint::ringStrokeWidth * dpr, color);
            EditorRhiGeometry::appendClippedTriangles(vertices, ringVertices, labelRect);
            topLeft.setX(block.textLeft);
        }
        const auto span = glyphAtlas.appendText(text, font, topLeft, color, labelRect, dpr,
                                                viewport.offset() * dpr, q->physicalWindowOffset());
        drawList.appendTexture(span, vertices.size());
    }

    void appendClipMask() {
        const auto visible = viewport.visibleSceneRect();
        const auto contentStart = viewport.tickToSceneX(content.contentStartTick());
        const auto contentEnd = viewport.tickToSceneX(content.contentEndTick());
        if (visible.left() < contentStart) {
            const auto right = std::min(visible.right(), contentStart);
            appendSceneRect(
                QRectF(visible.left(), visible.top(), right - visible.left(), visible.height()),
                clipRangeOverlayColor);
        }
        if (visible.right() > contentEnd) {
            const auto left = std::max(visible.left(), contentEnd);
            appendSceneRect(QRectF(left, visible.top(), visible.right() - left, visible.height()),
                            clipRangeOverlayColor);
        }
    }

    void appendLastPlaybackIndicator() {
        const auto visible = viewport.visibleSceneRect();
        const auto x = viewport.tickToSceneX(lastPlaybackPosition);
        for (auto top = visible.top(); top < visible.bottom(); top += 6.0) {
            appendPixelAlignedVerticalLine(x, top, std::min(top + 4.0, visible.bottom()),
                                           lastPlayPosIndicatorColor);
        }
    }

    AudioViewRhiWidget *q = nullptr;
    EditorViewportController viewport;
    EditorWheelController wheel;
    EditorRhiScrollBarController *scrollBars = nullptr;
    TimelineLineEmitter timelineEmitter;
    AudioViewContent content;
    std::vector<std::unique_ptr<AudioWaveformSampler>> samplers;
    EditorGlyphAtlas glyphAtlas;
    EditorTouchController *touchController = nullptr;
    // Turns the loading ring, stopped as soon as the clip has something to draw
    QTimer hintTicker;

    QVector<EditorRhiSolidVertex> vertices;
    EditorRhiDrawList drawList;
    QVector<EditorRhiOverlayRect> playbackOverlayRects;
    double dpr = 1.0;
    double playbackPosition = 0.0;
    double lastPlaybackPosition = 0.0;
    bool snapshotScheduled = false;
    bool viewportPositionPending = false;

    QColor laneBackgroundColor = {32, 35, 43};
    QColor laneSeparatorColor = {60, 65, 78};
    QColor waveformCenterLineColor = {60, 65, 78};
    QColor clipRangeOverlayColor = {0, 0, 0, 64};
    QColor barLineColor = {8, 9, 10};
    QColor beatLineColor = {22, 25, 28};
    QColor commonLineColor = {28, 32, 36};
    QColor playPosIndicatorColor = {255, 255, 255};
    QColor lastPlayPosIndicatorColor = {120, 120, 120};
    QColor emptyViewBackgroundColor = {24, 26, 32};
};

AudioViewRhiWidget::AudioViewRhiWidget(QWidget *parent)
    : EditorRhiWidget(QStringLiteral("AudioViewRhi"), parent), d(std::make_unique<Private>(this)) {
    setObjectName(QStringLiteral("AudioViewRhiWidget"));
    // Without this the platform turns fingers into mouse events and the touch controller
    // never sees a gesture, so pinch and two-finger pan would do nothing
    setAttribute(Qt::WA_AcceptTouchEvents);
    d->initializeScrollBars();
    // Two-finger pan and pinch, and a one-finger drag that pans: the canvas is
    // view-only, so no finger ever edits it. Creating it here rather than inside
    // Private matters: parenting a QObject sends the widget an event, and during
    // Private's construction the d member is not assigned yet
    d->touchController = new EditorTouchController(this, this);
    connect(this, &EditorRhiWidget::backendFailed, this,
            [this](const QString &) { emit backendUnavailable(); });
    connect(appModel, &AppModel::timelineChanged, this, [this] { d->scheduleSnapshot(); });
    connect(appStatus, &AppStatus::pianoRollQuantizeChanged, this,
            [this] { d->scheduleSnapshot(); });
}

AudioViewRhiWidget::~AudioViewRhiWidget() = default;

void AudioViewRhiWidget::setContent(const AudioViewContent &content) {
    d->setContent(content);
}

const AudioViewContent &AudioViewRhiWidget::content() const {
    return d->content;
}

double AudioViewRhiWidget::startTick() const {
    return d->viewport.startTick();
}

double AudioViewRhiWidget::endTick() const {
    return d->viewport.endTick();
}

double AudioViewRhiWidget::viewCenterTick() const {
    return (startTick() + endTick()) * 0.5;
}

double AudioViewRhiWidget::scaleX() const {
    return d->viewport.horizontalScale();
}

int AudioViewRhiWidget::horizontalBarValue() const {
    return qRound(d->viewport.horizontalOffset());
}

bool AudioViewRhiWidget::centerAt(const double center) {
    if (!std::isfinite(center) || center < 0.0)
        return false;
    d->wheel.stop();
    return d->viewport.centerAt(center, 0.0);
}

bool AudioViewRhiWidget::setTimeViewport(const double center, const double horizontalScale) {
    if (!std::isfinite(center) || center < 0.0 || !std::isfinite(horizontalScale) ||
        horizontalScale <= 0.0) {
        return false;
    }
    d->wheel.stop();
    const auto anchor = QPointF(width() * 0.5, height() * 0.5);
    const auto previousScale = d->viewport.horizontalScale();
    const auto previousCenter = viewCenterTick();
    if (!d->viewport.setScale(horizontalScale, 1.0, anchor))
        return false;
    if (centerAt(center))
        return true;
    d->viewport.setScale(previousScale, 1.0, anchor);
    centerAt(previousCenter);
    return false;
}

void AudioViewRhiWidget::positionViewportAtContent() {
    d->positionViewportAtContent();
    d->scheduleSnapshot();
}

void AudioViewRhiWidget::setHorizontalBarValue(const int value) {
    d->wheel.stop();
    d->viewport.setOffset({static_cast<double>(value), d->viewport.verticalOffset()});
}

void AudioViewRhiWidget::setPlaybackPosition(const double tick) {
    d->playbackPosition = tick;
    if (!d->snapshotScheduled)
        d->updatePlaybackOverlay();
}

void AudioViewRhiWidget::setLastPlaybackPosition(const double tick) {
    d->lastPlaybackPosition = tick;
    d->scheduleSnapshot();
}

void AudioViewRhiWidget::onWheelHorScale(QWheelEvent *event) {
    if (d->content.isValid())
        d->wheel.horizontalScale(event);
}

void AudioViewRhiWidget::onWheelHorScroll(QWheelEvent *event) {
    d->wheel.horizontalScroll(event);
}

void AudioViewRhiWidget::stopTouchViewportAnimation() {
    d->viewport.stopAnimation();
}

void AudioViewRhiWidget::panTouchViewportBy(const QPointF &deltaPixels) {
    // Only the time axis can move: the lanes always fill the canvas
    d->viewport.scrollBy(QPointF(-deltaPixels.x(), 0.0));
}

void AudioViewRhiWidget::zoomTouchViewportBy(const double horizontalFactor,
                                             const double verticalFactor, const QPointF &anchor) {
    Q_UNUSED(verticalFactor)
    d->viewport.setScale(d->viewport.horizontalScale() * horizontalFactor, 1.0, anchor);
}

EditorTouchTarget::ContentHit
    AudioViewRhiWidget::touchContentAt(const QPointF &viewportPosition) const {
    Q_UNUSED(viewportPosition)
    return ContentHit::None;
}

EditorTouchTarget::BlankDragAction AudioViewRhiWidget::touchBlankDragAction() const {
    return BlankDragAction::Pan;
}

bool AudioViewRhiWidget::touchFingerEdits() const {
    return false;
}

bool AudioViewRhiWidget::event(QEvent *event) {
    // The gesture layer sees a touch stream before anything else can turn it into
    // a mouse event, and a window that loses focus ends whatever gesture was running
    if (d->touchController && d->touchController->handleEvent(event))
        return true;
    if (event->type() == QEvent::WindowDeactivate && d->touchController)
        d->touchController->cancel();
    return EditorRhiWidget::event(event);
}

void AudioViewRhiWidget::showEvent(QShowEvent *event) {
    EditorRhiWidget::showEvent(event);
    d->positionViewportAtContent();
    d->scheduleSnapshot();
}

void AudioViewRhiWidget::hideEvent(QHideEvent *event) {
    d->wheel.stop();
    d->viewport.stopAnimation();
    EditorRhiWidget::hideEvent(event);
}

void AudioViewRhiWidget::resizeEvent(QResizeEvent *event) {
    EditorRhiWidget::resizeEvent(event);
    d->viewport.setViewportSize(size());
    d->positionViewportAtContent();
    d->updateScrollBars();
    d->scheduleSnapshot();
}

void AudioViewRhiWidget::wheelEvent(QWheelEvent *event) {
    if (!d->wheel.handleWheel(event))
        EditorRhiWidget::wheelEvent(event);
}

void AudioViewRhiWidget::onRhiReady() {
    d->scheduleSnapshot();
}

void AudioViewRhiWidget::onDevicePixelRatioChanged() {
    EditorRhiWidget::onDevicePixelRatioChanged();
    d->scheduleSnapshot();
}

void AudioViewRhiWidget::notifyViewportChanged() {
    emit timeRangeChanged(startTick(), endTick());
    emit scaleChanged(scaleX());
    emit horizontalBarValueChanged(horizontalBarValue());
}

QColor AudioViewRhiWidget::laneBackgroundColor() const {
    return d->laneBackgroundColor;
}

void AudioViewRhiWidget::setLaneBackgroundColor(const QColor &color) {
    d->laneBackgroundColor = color;
    d->scheduleSnapshot();
}

QColor AudioViewRhiWidget::laneSeparatorColor() const {
    return d->laneSeparatorColor;
}

void AudioViewRhiWidget::setLaneSeparatorColor(const QColor &color) {
    d->laneSeparatorColor = color;
    d->scheduleSnapshot();
}

QColor AudioViewRhiWidget::waveformCenterLineColor() const {
    return d->waveformCenterLineColor;
}

void AudioViewRhiWidget::setWaveformCenterLineColor(const QColor &color) {
    d->waveformCenterLineColor = color;
    d->scheduleSnapshot();
}

QColor AudioViewRhiWidget::clipRangeOverlayColor() const {
    return d->clipRangeOverlayColor;
}

void AudioViewRhiWidget::setClipRangeOverlayColor(const QColor &color) {
    d->clipRangeOverlayColor = color;
    d->scheduleSnapshot();
}

QColor AudioViewRhiWidget::barLineColor() const {
    return d->barLineColor;
}

void AudioViewRhiWidget::setBarLineColor(const QColor &color) {
    d->barLineColor = color;
    d->scheduleSnapshot();
}

QColor AudioViewRhiWidget::beatLineColor() const {
    return d->beatLineColor;
}

void AudioViewRhiWidget::setBeatLineColor(const QColor &color) {
    d->beatLineColor = color;
    d->scheduleSnapshot();
}

QColor AudioViewRhiWidget::commonLineColor() const {
    return d->commonLineColor;
}

void AudioViewRhiWidget::setCommonLineColor(const QColor &color) {
    d->commonLineColor = color;
    d->scheduleSnapshot();
}

QColor AudioViewRhiWidget::playPosIndicatorColor() const {
    return d->playPosIndicatorColor;
}

void AudioViewRhiWidget::setPlayPosIndicatorColor(const QColor &color) {
    d->playPosIndicatorColor = color;
    d->updatePlaybackOverlay();
}

QColor AudioViewRhiWidget::lastPlayPosIndicatorColor() const {
    return d->lastPlayPosIndicatorColor;
}

void AudioViewRhiWidget::setLastPlayPosIndicatorColor(const QColor &color) {
    d->lastPlayPosIndicatorColor = color;
    d->scheduleSnapshot();
}

QColor AudioViewRhiWidget::emptyViewBackgroundColor() const {
    return d->emptyViewBackgroundColor;
}

void AudioViewRhiWidget::setEmptyViewBackgroundColor(const QColor &color) {
    d->emptyViewBackgroundColor = color;
    d->scheduleSnapshot();
}
