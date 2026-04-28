//
// Created by fluty on 2023/11/16.
//

#include "AudioClipView.h"

#include <QElapsedTimer>
#include <QPainter>
#include <QThread>
#include <QFileDialog>

#include "Global/TracksEditorGlobal.h"

using namespace TracksEditorGlobal;

AudioClipView::AudioClipView(const int itemId, QGraphicsItem *parent)
    : AbstractClipView(itemId, parent) {
}

QString AudioClipView::path() const {
    return m_path;
}

void AudioClipView::setPath(const QString &path) {
    m_path = path;
    m_status = AppGlobal::Loaded;
}

double AudioClipView::tempo() const {
    return m_tempo;
}

void AudioClipView::setTempo(const double tempo) {
    m_tempo = tempo;
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

void AudioClipView::onTempoChange(const double tempo) {
    m_tempo = tempo;
}

// Draws the audio waveform directly on the painter (no off-screen pixmap).
//
// Key design decisions:
//   1. Direct rendering — avoids QPixmap allocation, fill, and blit overhead that
//      scales with clip area (especially costly at high DPI).
//   2. Physical-pixel stepping — iterates at 1/dpr logical-pixel intervals so that
//      every physical pixel column gets a waveform line, eliminating gaps on HiDPI.
//   3. Scene-aligned grid — the pixel step grid is anchored to scene coordinate 0
//      (via floor/ceil alignment) rather than to the clip's left edge. This ensures
//      that trimming (changing clipStart/clipLen) does not shift the sampling grid,
//      which would cause visible waveform jitter.
//   4. Absolute coordinate mapping — each pixel column's chunk range is computed from
//      its absolute scene X position (sceneX -> tick -> chunkIndex), so the mapping
//      is independent of previewRect width. This prevents jitter when the clip is
//      resized by trimming.
//   5. Batch drawLines — all vertical lines are collected into a QVector<QLineF> and
//      drawn in a single QPainter::drawLines() call to minimize state-switch overhead.
//   6. Cosmetic pen (width 0) — always renders as exactly 1 physical pixel regardless
//      of transform, which is the fastest line drawing mode in Qt.
void AudioClipView::drawPreviewArea(QPainter *painter, const QRectF &previewRect,
                                    const QColor color) {
    QElapsedTimer mstimer;
    mstimer.start();

    const auto rectLeft = previewRect.left();
    const auto rectTop = previewRect.top();
    const auto rectWidth = previewRect.width();
    const auto rectHeight = previewRect.height();
    const auto halfRectHeight = rectHeight / 2;

    painter->setRenderHint(QPainter::Antialiasing, false);

    QPen pen;

    if (m_status == AppGlobal::Loading) {
        pen.setColor(color);
        painter->setPen(pen);
        painter->drawText(previewRect, "Loading...", QTextOption(Qt::AlignCenter));
    }

    if (m_audioInfo.peakCache.count() == 0 || m_audioInfo.peakCacheMipmap.count() == 0)
        return;

    // Cosmetic pen: width 0 means always 1 physical pixel, fastest drawing mode
    pen.setColor(color);
    pen.setWidthF(0);
    painter->setPen(pen);

    // Select resolution level: use full-resolution peak data when zoomed in,
    // downsampled mipmap when zoomed out
    m_resolution = scaleX() >= 0.2 ? High : Low;
    const auto chunksPerTickBase = static_cast<double>(m_audioInfo.sampleRate) /
                                   m_audioInfo.chunkSize * 60 / m_tempo /
                                   AppGlobal::ticksPerQuarterNote;
    const auto &peakData =
        m_resolution == Low ? m_audioInfo.peakCacheMipmap : m_audioInfo.peakCache;
    const auto chunksPerTick =
        m_resolution == Low ? chunksPerTickBase / m_audioInfo.mipmapScale : chunksPerTickBase;

    // Determine the visible drawing range in scene coordinates
    const auto rectLeftScene = mapToScene(previewRect.topLeft()).x();
    const auto rectRightScene = mapToScene(previewRect.bottomRight()).x();
    const qreal dpr = painter->device()->devicePixelRatio();
    const auto visLeft = visibleRect().left();
    const auto visRight = visibleRect().right();
    const auto drawLeftScene = std::max(visLeft, rectLeftScene);
    const auto drawRightScene = std::min(visRight, rectRightScene);

    if (drawLeftScene >= drawRightScene)
        return;

    // Conversion factor: how many ticks correspond to one scene pixel
    const double ticksPerScenePixel =
        AppGlobal::ticksPerQuarterNote / (scaleX() * pixelsPerQuarterNote);
    // Step size in logical pixels: 1/dpr so we iterate per physical pixel
    const double pixelStep = 1.0 / dpr;
    // How many peak chunks each physical pixel step covers
    const double chunksPerStep = ticksPerScenePixel * pixelStep * chunksPerTick;
    const int peakCount = peakData.count();

    // Align the drawing range to the global pixel grid (anchored at scene X = 0).
    // This prevents waveform jitter when trimming clip edges, because the sampling
    // grid positions remain stable regardless of where drawLeftScene falls.
    const double alignedDrawLeftScene =
        std::floor(drawLeftScene / pixelStep) * pixelStep;
    const double alignedDrawRightScene =
        std::ceil(drawRightScene / pixelStep) * pixelStep;
    const int stepCount =
        static_cast<int>((alignedDrawRightScene - alignedDrawLeftScene) / pixelStep) + 1;

    QVector<QLineF> lines;
    lines.reserve(stepCount);

    for (int i = 0; i < stepCount; i++) {
        // Compute absolute scene X for this physical pixel column
        const double sceneX = alignedDrawLeftScene + i * pixelStep;
        const double logicalX = sceneX - rectLeftScene;
        // Skip columns outside the clip's previewRect bounds
        if (logicalX < 0 || logicalX > rectWidth)
            continue;

        // Map scene position to peak data index via absolute coordinates:
        // sceneX -> tick -> chunk index
        // This is independent of rectWidth, so trimming doesn't affect the mapping.
        const double tick = sceneX * ticksPerScenePixel;
        const double chunkPos = tick * chunksPerTick;
        const double chunkEnd = chunkPos + chunksPerStep;

        // Find min/max peak values within this pixel's chunk range
        short min = 0;
        short max = 0;

        const int jStart = static_cast<int>(chunkPos);
        const int jEnd = static_cast<int>(chunkEnd);
        for (int j = jStart; j < jEnd; j++) {
            if (j < 0)
                continue;
            if (j >= peakCount)
                break;
            const auto &frame = peakData.at(j);
            const auto frameMin = std::get<0>(frame);
            const auto frameMax = std::get<1>(frame);
            if (frameMin < min)
                min = frameMin;
            if (frameMax > max)
                max = frameMax;
        }

        // Convert peak values to Y coordinates and add vertical line
        const double x = rectLeft + logicalX;
        const double yMin = -min * halfRectHeight / 32767.0 + halfRectHeight + rectTop;
        const double yMax = -max * halfRectHeight / 32767.0 + halfRectHeight + rectTop;
        lines.append(QLineF(x, yMin, x, yMax));
    }

    // Batch draw all waveform lines in one call for optimal performance
    painter->drawLines(lines);

    // const auto time = static_cast<double>(mstimer.nsecsElapsed()) / 1000000.0;
    // qDebug() << "AudioClipView waveform render:" << time << "ms";
}

QString AudioClipView::clipTypeName() const {
    return tr("[Audio] ");
}

QString AudioClipView::iconPath() const {
    return ":svg/icons/audio_clip_16_filled.svg";
}
