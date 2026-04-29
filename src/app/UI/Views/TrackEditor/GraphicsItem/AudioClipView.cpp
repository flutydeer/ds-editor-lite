//
// Created by fluty on 2023/11/16.
//

#include "AudioClipView.h"

#include <QElapsedTimer>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

#include "Global/TracksEditorGlobal.h"
#include "Modules/Audio/AudioContext.h"

#include <TalcsFormat/FormatManager.h>
#include <TalcsFormat/AbstractAudioFormatIO.h>

using namespace TracksEditorGlobal;

static constexpr int kSincHalfKernel = 16;
static constexpr double kCurveThreshold = 4.0;
static constexpr int kCurveOversample = 3;

AudioClipView::AudioClipView(const int itemId, QGraphicsItem *parent)
    : AbstractClipView(itemId, parent) {
}

AudioClipView::~AudioClipView() {
    resetIO();
}

QString AudioClipView::path() const {
    return m_path;
}

void AudioClipView::setPath(const QString &path) {
    if (m_path != path)
        resetIO();
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

bool AudioClipView::ensureIO() {
    if (m_io)
        return true;
    if (m_path.isEmpty())
        return false;
    auto *fm = AudioContext::instance()->formatManager();
    if (!fm)
        return false;
    m_io = fm->getFormatLoad(m_path);
    if (!m_io)
        return false;
    if (!m_io->open(talcs::AbstractAudioFormatIO::Read)) {
        delete m_io;
        m_io = nullptr;
        return false;
    }
    return true;
}

void AudioClipView::resetIO() {
    if (m_io) {
        m_io->close();
        delete m_io;
        m_io = nullptr;
    }
}

double AudioClipView::sincInterpolate(const QVector<float> &samples, const qint64 offset,
                                      const qint64 totalFrames, const double position,
                                      const int halfKernel) {
    const int center = static_cast<int>(std::floor(position));
    const double frac = position - center;

    double result = 0.0;
    for (int i = -halfKernel; i <= halfKernel; i++) {
        const qint64 idx = static_cast<qint64>(center) + i;
        if (idx < 0 || idx >= totalFrames)
            continue;
        const int bufIdx = static_cast<int>(idx - offset);
        if (bufIdx < 0 || bufIdx >= samples.size())
            continue;

        const double x = frac - i;
        double sinc_val;
        double window;
        if (std::abs(x) < 1e-9) {
            sinc_val = 1.0;
            window = 1.0;
        } else {
            sinc_val = std::sin(M_PI * x) / (M_PI * x);
            const double wx = x / halfKernel;
            window = (std::abs(wx) < 1.0) ? std::sin(M_PI * wx) / (M_PI * wx) : 0.0;
        }
        result += samples[bufIdx] * sinc_val * window;
    }
    return result;
}

// Dispatches to the appropriate rendering method based on zoom level.
//
// samplesPerPixel thresholds:
//   > chunkSize  → peak mipmap / peakCache (vertical lines, batch drawLines)
//   > ~4         → sub-chunk peak mode (IO seek+read, per-pixel min/max lines)
//   ≤ ~4         → waveform curve mode (IO seek+read, Lanczos sinc interpolation, QPainterPath)
void AudioClipView::drawPreviewArea(QPainter *painter, const QRectF &previewRect,
                                    const QColor color) {
    if (m_status == AppGlobal::Loading) {
        QPen pen;
        pen.setColor(color);
        painter->setPen(pen);
        painter->drawText(previewRect, "Loading...", QTextOption(Qt::AlignCenter));
    }

    if (m_audioInfo.sampleRate <= 0 || m_audioInfo.chunkSize <= 0)
        return;

    const double ticksPerScenePixel =
        AppGlobal::ticksPerQuarterNote / (scaleX() * pixelsPerQuarterNote);
    const double samplesPerTick = static_cast<double>(m_audioInfo.sampleRate) * 60.0 /
                                  m_tempo / AppGlobal::ticksPerQuarterNote;
    const qreal dpr = painter->device()->devicePixelRatio();
    const double pixelStep = 1.0 / dpr;
    const double samplesPerPixel = ticksPerScenePixel * pixelStep * samplesPerTick;
    const double chunkSize = static_cast<double>(m_audioInfo.chunkSize);

    if (samplesPerPixel <= chunkSize) {
        if (samplesPerPixel <= kCurveThreshold)
            drawWaveformCurve(painter, previewRect, color);
        else
            drawSubChunkPeakMode(painter, previewRect, color);
    } else {
        drawPeakMode(painter, previewRect, color);
    }
}

// Peak mode: uses pre-computed peakCache / peakCacheMipmap.
// Physical-pixel stepping + scene-aligned grid + batch drawLines.
void AudioClipView::drawPeakMode(QPainter *painter, const QRectF &previewRect,
                                 const QColor &color) {
    if (m_audioInfo.peakCache.count() == 0 || m_audioInfo.peakCacheMipmap.count() == 0)
        return;

    // Cosmetic pen: width 0 means always 1 physical pixel, fastest drawing mode
    const auto rectLeft = previewRect.left();
    const auto rectTop = previewRect.top();
    const auto rectWidth = previewRect.width();
    const auto rectHeight = previewRect.height();
    const auto halfRectHeight = rectHeight / 2;

    painter->setRenderHint(QPainter::Antialiasing, false);
    QPen pen;
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

        const double tick = sceneX * ticksPerScenePixel;
        const double audioTick = tick - start();
        const double chunkPos = audioTick * chunksPerTick;
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
}

// Sub-chunk peak mode: reads raw samples via IO, computes per-pixel min/max.
// Used when zoomed in enough that each pixel covers fewer samples than chunkSize
// but still more than ~4 samples.
void AudioClipView::drawSubChunkPeakMode(QPainter *painter, const QRectF &previewRect,
                                         const QColor &color) {
    if (!ensureIO())
        return;

    const auto rectLeft = previewRect.left();
    const auto rectTop = previewRect.top();
    const auto rectWidth = previewRect.width();
    const auto rectHeight = previewRect.height();
    const auto halfRectHeight = rectHeight / 2;

    painter->setRenderHint(QPainter::Antialiasing, false);
    QPen pen;
    pen.setColor(color);
    pen.setWidthF(0);
    painter->setPen(pen);

    const auto rectLeftScene = mapToScene(previewRect.topLeft()).x();
    const auto rectRightScene = mapToScene(previewRect.bottomRight()).x();
    const qreal dpr = painter->device()->devicePixelRatio();
    const auto visLeft = visibleRect().left();
    const auto visRight = visibleRect().right();
    const auto drawLeftScene = std::max(visLeft, rectLeftScene);
    const auto drawRightScene = std::min(visRight, rectRightScene);

    if (drawLeftScene >= drawRightScene)
        return;

    const double ticksPerScenePixel =
        AppGlobal::ticksPerQuarterNote / (scaleX() * pixelsPerQuarterNote);
    const double samplesPerTick = static_cast<double>(m_audioInfo.sampleRate) * 60.0 /
                                  m_tempo / AppGlobal::ticksPerQuarterNote;
    const double pixelStep = 1.0 / dpr;
    const double samplesPerStep = ticksPerScenePixel * pixelStep * samplesPerTick;
    const int channels = m_audioInfo.channels;
    const qint64 totalFrames = m_audioInfo.frames;

    const double alignedDrawLeftScene =
        std::floor(drawLeftScene / pixelStep) * pixelStep;
    const double alignedDrawRightScene =
        std::ceil(drawRightScene / pixelStep) * pixelStep;
    const int stepCount =
        static_cast<int>((alignedDrawRightScene - alignedDrawLeftScene) / pixelStep) + 1;

    const double startTick = static_cast<double>(start());

    const double firstTick = alignedDrawLeftScene * ticksPerScenePixel - startTick;
    const double lastTick = alignedDrawRightScene * ticksPerScenePixel - startTick;
    qint64 sampleStart = static_cast<qint64>(std::floor(firstTick * samplesPerTick));
    qint64 sampleEnd = static_cast<qint64>(std::ceil(lastTick * samplesPerTick + samplesPerStep));
    sampleStart = std::max(sampleStart, qint64(0));
    sampleEnd = std::min(sampleEnd, totalFrames);

    if (sampleEnd <= sampleStart)
        return;

    const qint64 framesToRead = sampleEnd - sampleStart;
    m_ioBuffer.resize(static_cast<int>(framesToRead * channels));

    m_io->seek(sampleStart);
    const qint64 framesRead = m_io->read(m_ioBuffer.data(), framesToRead);
    if (framesRead <= 0)
        return;

    QVector<QLineF> lines;
    lines.reserve(stepCount);

    for (int i = 0; i < stepCount; i++) {
        const double sceneX = alignedDrawLeftScene + i * pixelStep;
        const double logicalX = sceneX - rectLeftScene;
        if (logicalX < 0 || logicalX > rectWidth)
            continue;

        const double tick = sceneX * ticksPerScenePixel;
        const double audioTick = tick - startTick;
        const double samplePos = audioTick * samplesPerTick;
        const double samplePosEnd = samplePos + samplesPerStep;

        qint64 jStart = static_cast<qint64>(std::floor(samplePos));
        qint64 jEnd = static_cast<qint64>(std::ceil(samplePosEnd));
        jStart = std::max(jStart, sampleStart);
        jEnd = std::min(jEnd, sampleStart + framesRead);

        float min = 0.0f;
        float max = 0.0f;

        for (qint64 j = jStart; j < jEnd; j++) {
            const int bufOffset = static_cast<int>((j - sampleStart) * channels);
            float mono = 0.0f;
            for (int ch = 0; ch < channels; ch++)
                mono += m_ioBuffer[bufOffset + ch];
            mono /= channels;
            if (mono < min) min = mono;
            if (mono > max) max = mono;
        }

        const double x = rectLeft + logicalX;
        const double yMin = -min * halfRectHeight + halfRectHeight + rectTop;
        const double yMax = -max * halfRectHeight + halfRectHeight + rectTop;
        lines.append(QLineF(x, yMin, x, yMax));
    }

    painter->drawLines(lines);
}

// Waveform curve mode: reads raw samples, applies Lanczos sinc interpolation,
// draws a smooth continuous curve via QPainterPath.
// Each physical pixel gets kCurveOversample interpolation points for smoothness.
// When sample points are far enough apart (> 6 logical pixels), draws filled circles
// at each actual sample position to show the discrete sampling grid.
void AudioClipView::drawWaveformCurve(QPainter *painter, const QRectF &previewRect,
                                      const QColor &color) {
    if (!ensureIO())
        return;

    const auto rectLeft = previewRect.left();
    const auto rectTop = previewRect.top();
    const auto rectWidth = previewRect.width();
    const auto rectHeight = previewRect.height();
    const auto halfRectHeight = rectHeight / 2;

    painter->setRenderHint(QPainter::Antialiasing, true);
    QPen pen;
    pen.setColor(color);
    pen.setWidthF(0);
    painter->setPen(pen);

    const auto rectLeftScene = mapToScene(previewRect.topLeft()).x();
    const auto rectRightScene = mapToScene(previewRect.bottomRight()).x();
    const qreal dpr = painter->device()->devicePixelRatio();
    const auto visLeft = visibleRect().left();
    const auto visRight = visibleRect().right();
    const auto drawLeftScene = std::max(visLeft, rectLeftScene);
    const auto drawRightScene = std::min(visRight, rectRightScene);

    if (drawLeftScene >= drawRightScene)
        return;

    const double ticksPerScenePixel =
        AppGlobal::ticksPerQuarterNote / (scaleX() * pixelsPerQuarterNote);
    const double samplesPerTick = static_cast<double>(m_audioInfo.sampleRate) * 60.0 /
                                  m_tempo / AppGlobal::ticksPerQuarterNote;
    const double pixelStep = 1.0 / dpr;
    const int channels = m_audioInfo.channels;
    const qint64 totalFrames = m_audioInfo.frames;

    const double alignedDrawLeftScene =
        std::floor(drawLeftScene / pixelStep) * pixelStep;
    const double alignedDrawRightScene =
        std::ceil(drawRightScene / pixelStep) * pixelStep;

    const double startTick = static_cast<double>(start());

    const double firstTick = alignedDrawLeftScene * ticksPerScenePixel - startTick;
    const double lastTick = alignedDrawRightScene * ticksPerScenePixel - startTick;
    qint64 sampleStart = static_cast<qint64>(std::floor(firstTick * samplesPerTick)) - kSincHalfKernel;
    qint64 sampleEnd = static_cast<qint64>(std::ceil(lastTick * samplesPerTick)) + kSincHalfKernel;
    sampleStart = std::max(sampleStart, qint64(0));
    sampleEnd = std::min(sampleEnd, totalFrames);

    if (sampleEnd <= sampleStart)
        return;

    const qint64 framesToRead = sampleEnd - sampleStart;
    m_ioBuffer.resize(static_cast<int>(framesToRead * channels));

    m_io->seek(sampleStart);
    const qint64 framesRead = m_io->read(m_ioBuffer.data(), framesToRead);
    if (framesRead <= 0)
        return;

    QVector<float> monoSamples(static_cast<int>(framesRead));
    for (int i = 0; i < framesRead; i++) {
        float mono = 0.0f;
        const int bufOffset = i * channels;
        for (int ch = 0; ch < channels; ch++)
            mono += m_ioBuffer[bufOffset + ch];
        mono /= channels;
        monoSamples[i] = mono;
    }

    const double subStep = pixelStep / kCurveOversample;
    const int totalPoints =
        static_cast<int>((alignedDrawRightScene - alignedDrawLeftScene) / subStep) + 1;

    QPainterPath path;
    bool started = false;

    for (int i = 0; i < totalPoints; i++) {
        const double sceneX = alignedDrawLeftScene + i * subStep;
        const double logicalX = sceneX - rectLeftScene;
        if (logicalX < 0 || logicalX > rectWidth)
            continue;

        const double tick = sceneX * ticksPerScenePixel;
        const double audioTick = tick - startTick;
        const double samplePos = audioTick * samplesPerTick;

        const double value = sincInterpolate(monoSamples, sampleStart, totalFrames,
                                             samplePos, kSincHalfKernel);

        const double x = rectLeft + logicalX;
        const double y = -value * halfRectHeight + halfRectHeight + rectTop;

        if (!started) {
            path.moveTo(x, y);
            started = true;
        } else {
            path.lineTo(x, y);
        }
    }

    painter->drawPath(path);

    const double samplesPerLogicalPixel = ticksPerScenePixel * samplesPerTick;
    if (samplesPerLogicalPixel < 1.0 / 6.0) {
        const double dotRadius = std::min(3.0, samplesPerLogicalPixel > 0 ? 1.0 / samplesPerLogicalPixel * 0.3 : 3.0);
        painter->setBrush(color);
        painter->setPen(Qt::NoPen);

        const qint64 firstSample = static_cast<qint64>(std::floor(firstTick * samplesPerTick));
        const qint64 lastSample = static_cast<qint64>(std::ceil(lastTick * samplesPerTick));

        for (qint64 s = std::max(firstSample, qint64(0)); s < std::min(lastSample, totalFrames); s++) {
            const int bufIdx = static_cast<int>(s - sampleStart);
            if (bufIdx < 0 || bufIdx >= monoSamples.size())
                continue;

            const double audioTick = static_cast<double>(s) / samplesPerTick;
            const double sceneX = (audioTick + startTick) / ticksPerScenePixel;
            const double logicalX = sceneX - rectLeftScene;
            if (logicalX < 0 || logicalX > rectWidth)
                continue;

            const double x = rectLeft + logicalX;
            const double y = -monoSamples[bufIdx] * halfRectHeight + halfRectHeight + rectTop;
            painter->drawEllipse(QPointF(x, y), dotRadius, dotRadius);
        }
    }
}

QString AudioClipView::clipTypeName() const {
    return tr("[Audio] ");
}

QString AudioClipView::iconPath() const {
    return ":svg/icons/audio_clip_16_filled.svg";
}
