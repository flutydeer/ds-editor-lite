#include "AudioClipView.h"

#include "Global/TracksEditorGlobal.h"
#include "UI/Utils/WaveformRenderUtils.h"
#include "UI/Views/Common/AudioLoadHint.h"

#include <QCoreApplication>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>

using namespace TracksEditorGlobal;

AudioClipView::AudioClipView(const int itemId, QGraphicsItem *parent)
    : AbstractClipView(itemId, parent) {
    m_hintTicker.setInterval(16);
    connect(&m_hintTicker, &QTimer::timeout, this, [this] { update(); });
    // The view starts out loading, and setLoadState() stops the ticker once there is a waveform
    m_hintTicker.start();
}

AudioClipView::~AudioClipView() = default;

QString AudioClipView::path() const {
    return m_path;
}

void AudioClipView::setPath(const QString &path) {
    m_path = path;
    m_waveformSampler.setPath(path);
}

void AudioClipView::setTimeline(const Timeline &timeline) {
    m_timeline = timeline;
    m_waveformSampler.invalidate();
    update();
}

void AudioClipView::setAudioInfo(const AudioInfoModel &info) {
    m_audioInfo = info;
    m_waveformSampler.invalidate();
    update();
}

void AudioClipView::setLoadState(const AppGlobal::AudioClipLoadState state) {
    if (m_loadState == state)
        return;
    m_loadState = state;
    // Only the loading hint animates, so nothing keeps repainting a clip whose waveform is drawn
    if (state == AppGlobal::AudioClipLoadState::Loading)
        m_hintTicker.start();
    else
        m_hintTicker.stop();
    update();
}

void AudioClipView::setErrorMessage(const QString &errorMessage) {
    m_errorMessage = errorMessage;
    update();
}

int AudioClipView::contentLength() const {
    return length();
}

void AudioClipView::setRenderMode(const WaveformRenderUtils::Mode mode) {
    m_renderMode = mode;
    update();
}

WaveformRenderUtils::Mode AudioClipView::renderMode() const {
    return m_renderMode;
}

void AudioClipView::drawPreviewArea(QPainter *painter, const QRectF &previewRect,
                                    const QColor color) {
    if (m_loadState != AppGlobal::AudioClipLoadState::Loaded) {
        const auto isMissing = m_loadState == AppGlobal::AudioClipLoadState::Missing;
        auto textColor = color;
        if (isMissing)
            textColor.setAlphaF(textColor.alphaF() * 0.5);
        auto text = isMissing ? QCoreApplication::translate("AudioClipView", "File missing")
                              : QCoreApplication::translate("AudioClipView", "Loading...");
        if (isMissing && !m_errorMessage.isEmpty())
            text += ": " + m_errorMessage;
        // Centre inside the visible part of the clip. A long clip scrolled to one of its ends
        // would otherwise draw the hint far outside the viewport and read as "no hint at all".
        auto labelRect = previewRect;
        const auto visible = visibleRect();
        if (!visible.isEmpty()) {
            const auto visibleInItem = mapRectFromScene(visible);
            const auto clipped = labelRect.intersected(visibleInItem);
            if (clipped.isEmpty())
                return;
            labelRect = clipped;
        }
        if (isMissing) {
            painter->setPen(textColor);
            painter->drawText(labelRect, text, QTextOption(Qt::AlignCenter));
            return;
        }
        drawLoadingHint(painter, labelRect, text, textColor);
        return;
    }

    const auto previewTopLeft = mapToScene(previewRect.topLeft());
    const auto previewBottomRight = mapToScene(previewRect.bottomRight());
    const auto waveform = m_waveformSampler.sample({
        .audioInfo = &m_audioInfo,
        .timeline = &m_timeline,
        .materialStartTick = start(),
        .visibleStartTick = start() + clipStart(),
        .previewSceneRect = QRectF(previewTopLeft, previewBottomRight),
        .visibleSceneRect = visibleRect(),
        .horizontalScale = scaleX(),
        .pixelsPerQuarterNote = pixelsPerQuarterNote,
        .leftMarginPx = leftMarginPx(),
        .devicePixelRatio = painter->deviceTransform().m11(),
    });

    bool invertible = false;
    const auto sceneToItem = sceneTransform().inverted(&invertible);
    if (!invertible)
        return;
    WaveformRenderUtils::renderWaveform(painter, color, m_renderMode, waveform, sceneToItem);
}

void AudioClipView::drawLoadingHint(QPainter *painter, const QRectF &labelRect, const QString &text,
                                    const QColor &textColor) {
    const QFontMetricsF metrics(painter->font());
    const auto textWidth = metrics.horizontalAdvance(text);
    const auto block = AudioLoadHint::layout(labelRect, textWidth, 1.0);
    const auto stroke = AudioLoadHint::ringStrokeWidth;

    // Stroke the same centreline the RHI backend feeds to its soft geometry, so both backends draw
    // one shape rather than two look-alikes
    const auto toPath = [](const QVector<QPointF> &points) {
        QPainterPath path;
        if (points.isEmpty())
            return path;
        path.moveTo(points.constFirst());
        for (qsizetype index = 1; index < points.size(); ++index)
            path.lineTo(points.at(index));
        return path;
    };

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);

    auto trackColor = textColor;
    trackColor.setAlphaF(trackColor.alphaF() * AudioLoadHint::trackAlpha);
    painter->setPen(QPen(trackColor, stroke, Qt::SolidLine, Qt::RoundCap));
    painter->drawPath(toPath(AudioLoadHint::trackCenterline(block.ringCenter, block.ringRadius)));

    painter->setPen(QPen(textColor, stroke, Qt::SolidLine, Qt::RoundCap));
    painter->drawPath(toPath(AudioLoadHint::arcCenterline(block.ringCenter, block.ringRadius,
                                                          AudioLoadHint::clockMs())));
    painter->restore();

    // The text sits to the right of the ring, inside the block the layout centred in labelRect
    painter->setPen(textColor);
    const auto textRect = QRectF(block.textLeft, labelRect.top(), textWidth, labelRect.height());
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
}

QString AudioClipView::clipTypeName() const {
    return QCoreApplication::translate("AudioClipView", "[Audio] ");
}

QString AudioClipView::iconPath() const {
    return ":svg/icons/audio_clip_16_filled.svg";
}
