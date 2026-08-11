#include "AudioClipView.h"

#include "Global/TracksEditorGlobal.h"
#include "UI/Utils/WaveformRenderUtils.h"

#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>

using namespace TracksEditorGlobal;

AudioClipView::AudioClipView(const int itemId, QGraphicsItem *parent)
    : AbstractClipView(itemId, parent) {
}

AudioClipView::~AudioClipView() = default;

QString AudioClipView::path() const {
    return m_path;
}

void AudioClipView::setPath(const QString &path) {
    m_path = path;
    m_waveformSampler.setPath(path);
    if (m_status != AppGlobal::Error)
        m_status = AppGlobal::Loaded;
}

void AudioClipView::setTimeline(const Timeline &timeline) {
    m_timeline = timeline;
    update();
}

void AudioClipView::setAudioInfo(const AudioInfoModel &info) {
    m_audioInfo = info;
    update();
}

void AudioClipView::setStatus(const AppGlobal::AudioLoadStatus status) {
    m_status = status;
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
    if (m_status == AppGlobal::Error) {
        auto dimmed = color;
        dimmed.setAlphaF(color.alphaF() * 0.5);
        painter->setPen(dimmed);
        auto text = QCoreApplication::translate("AudioClipView", "File missing");
        if (!m_errorMessage.isEmpty())
            text += ": " + m_errorMessage;
        painter->drawText(previewRect, text, QTextOption(Qt::AlignCenter));
        return;
    }
    if (m_status == AppGlobal::Loading) {
        painter->setPen(color);
        painter->drawText(previewRect, "Loading...", QTextOption(Qt::AlignCenter));
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

    if (waveform.geometry == AudioWaveformSampler::Geometry::FilledPeaks ||
        waveform.geometry == AudioWaveformSampler::Geometry::VerticalPeaks) {
        QVector<WaveformRenderUtils::PeakPoint> localPeaks;
        localPeaks.reserve(waveform.peaks.size());
        for (const auto &point : waveform.peaks) {
            const auto minimum = mapFromScene(QPointF(point.x, point.yMin));
            const auto maximum = mapFromScene(QPointF(point.x, point.yMax));
            localPeaks.append({minimum.x(), minimum.y(), maximum.y()});
        }
        const auto mode = waveform.geometry == AudioWaveformSampler::Geometry::FilledPeaks
                              ? m_renderMode
                              : WaveformRenderUtils::LineMode;
        WaveformRenderUtils::renderWaveform(painter, color, mode, localPeaks);
        return;
    }

    if (waveform.geometry != AudioWaveformSampler::Geometry::Curve || waveform.curve.isEmpty())
        return;
    painter->setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color);
    pen.setWidthF(0.0);
    painter->setPen(pen);
    QPainterPath path;
    path.moveTo(mapFromScene(waveform.curve.constFirst()));
    for (auto index = 1; index < waveform.curve.size(); ++index)
        path.lineTo(mapFromScene(waveform.curve[index]));
    painter->drawPath(path);

    if (waveform.sampleDots.isEmpty())
        return;
    painter->setBrush(color);
    painter->setPen(Qt::NoPen);
    for (const auto &point : waveform.sampleDots) {
        painter->drawEllipse(mapFromScene(point), waveform.sampleDotRadius,
                             waveform.sampleDotRadius);
    }
}

QString AudioClipView::clipTypeName() const {
    return QCoreApplication::translate("AudioClipView", "[Audio] ");
}

QString AudioClipView::iconPath() const {
    return ":svg/icons/audio_clip_16_filled.svg";
}
