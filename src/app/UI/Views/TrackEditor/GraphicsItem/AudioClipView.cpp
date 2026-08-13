#include "AudioClipView.h"

#include "Global/TracksEditorGlobal.h"
#include "UI/Utils/WaveformRenderUtils.h"

#include <QCoreApplication>
#include <QPainter>

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
    m_waveformSampler.invalidate();
    update();
}

void AudioClipView::setAudioInfo(const AudioInfoModel &info) {
    m_audioInfo = info;
    m_waveformSampler.invalidate();
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

    bool invertible = false;
    const auto sceneToItem = sceneTransform().inverted(&invertible);
    if (!invertible)
        return;
    WaveformRenderUtils::renderWaveform(painter, color, m_renderMode, waveform, sceneToItem);
}

QString AudioClipView::clipTypeName() const {
    return QCoreApplication::translate("AudioClipView", "[Audio] ");
}

QString AudioClipView::iconPath() const {
    return ":svg/icons/audio_clip_16_filled.svg";
}
