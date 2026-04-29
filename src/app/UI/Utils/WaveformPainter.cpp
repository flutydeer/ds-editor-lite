//
// Created by assistant on 2025/04/29.
//

#include "WaveformPainter.h"

#include "Modules/Audio/AudioContext.h"

#include <QPainter>
#include <cmath>

#include <TalcsFormat/FormatManager.h>
#include <TalcsFormat/AbstractAudioFormatIO.h>

#include "Global/AppGlobal.h"

WaveformPainter::WaveformPainter() = default;

WaveformPainter::~WaveformPainter() {
    resetIO();
}

void WaveformPainter::setAudioPath(const QString &path) {
    if (m_path != path)
        resetIO();
    m_path = path;
}

void WaveformPainter::setAudioInfo(const AudioInfoModel &info) {
    m_audioInfo = info;
}

void WaveformPainter::setTempo(const double tempo) {
    m_tempo = tempo;
}

void WaveformPainter::paint(QPainter *painter, const QRectF &rect, const QColor &color,
                            const double rectStartTick, const double rectEndTick) {
    if (m_audioInfo.sampleRate <= 0 || m_audioInfo.chunkSize <= 0)
        return;
    if (rect.width() <= 0 || rect.height() <= 0)
        return;

    const double ticksPerPixel = (rectEndTick - rectStartTick) / rect.width();
    const double samplesPerTick = static_cast<double>(m_audioInfo.sampleRate) * 60.0 /
                                  m_tempo / AppGlobal::ticksPerQuarterNote;
    const qreal dpr = painter->device()->devicePixelRatio();
    const double pixelStep = 1.0 / dpr;
    const double samplesPerPixel = ticksPerPixel * pixelStep * samplesPerTick;
    const double chunkSize = static_cast<double>(m_audioInfo.chunkSize);

    if (samplesPerPixel <= chunkSize)
        drawSubChunkPeakMode(painter, rect, color, rectStartTick, ticksPerPixel, samplesPerTick);
    else
        drawPeakMode(painter, rect, color, rectStartTick, ticksPerPixel, samplesPerTick);
}

bool WaveformPainter::ensureIO() {
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

void WaveformPainter::resetIO() {
    if (m_io) {
        m_io->close();
        delete m_io;
        m_io = nullptr;
    }
}

void WaveformPainter::drawPeakMode(QPainter *painter, const QRectF &rect, const QColor &color,
                                    const double rectStartTick, const double ticksPerPixel,
                                    const double samplesPerTick) {
    if (m_audioInfo.peakCache.count() == 0 || m_audioInfo.peakCacheMipmap.count() == 0)
        return;

    const auto rectLeft = rect.left();
    const auto rectTop = rect.top();
    const auto rectWidth = rect.width();
    const auto rectHeight = rect.height();
    const auto halfRectHeight = rectHeight / 2;

    painter->setRenderHint(QPainter::Antialiasing, false);
    QPen pen;
    pen.setColor(color);
    pen.setWidthF(0);
    painter->setPen(pen);

    const auto chunksPerTickBase = static_cast<double>(m_audioInfo.sampleRate) /
                                   m_audioInfo.chunkSize * 60 / m_tempo /
                                   AppGlobal::ticksPerQuarterNote;

    const qreal dpr = painter->device()->devicePixelRatio();
    const double pixelStep = 1.0 / dpr;

    const bool lowRes = ticksPerPixel > 5.0;
    const auto &peakData = lowRes ? m_audioInfo.peakCacheMipmap : m_audioInfo.peakCache;
    const auto chunksPerTick =
        lowRes ? chunksPerTickBase / m_audioInfo.mipmapScale : chunksPerTickBase;

    const double chunksPerStep = ticksPerPixel * pixelStep * chunksPerTick;
    const int peakCount = peakData.count();
    const int stepCount = static_cast<int>(std::ceil(rectWidth / pixelStep)) + 1;

    QVector<QLineF> lines;
    lines.reserve(stepCount);

    for (int i = 0; i < stepCount; i++) {
        const double localX = i * pixelStep;
        if (localX > rectWidth)
            break;

        const double tick = rectStartTick + localX * ticksPerPixel;
        const double relativeTick = tick - rectStartTick;
        const double chunkPos = relativeTick * chunksPerTick;
        const double chunkEnd = chunkPos + chunksPerStep;

        short min = 0;
        short max = 0;

        const int jStart = static_cast<int>(std::floor(chunkPos));
        const int jEnd = std::max(jStart + 1, static_cast<int>(std::ceil(chunkEnd)));
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

        const double x = rectLeft + localX;
        const double yMin = -logAmplify(min / 32767.0) * halfRectHeight + halfRectHeight + rectTop;
        const double yMax = -logAmplify(max / 32767.0) * halfRectHeight + halfRectHeight + rectTop;
        lines.append(QLineF(x, yMin, x, yMax));
    }

    painter->drawLines(lines);
}

double WaveformPainter::logAmplify(const double value) {
    constexpr double k = 15.0;
    static const double norm = 1.0 / std::log(1.0 + k);
    if (value >= 0)
        return std::log(1.0 + k * value) * norm;
    return -std::log(1.0 - k * value) * norm;
}

void WaveformPainter::drawSubChunkPeakMode(QPainter *painter, const QRectF &rect,
                                            const QColor &color, const double rectStartTick,
                                            const double ticksPerPixel,
                                            const double samplesPerTick) {
    if (!ensureIO())
        return;

    const auto rectLeft = rect.left();
    const auto rectTop = rect.top();
    const auto rectWidth = rect.width();
    const auto rectHeight = rect.height();
    const auto halfRectHeight = rectHeight / 2;

    painter->setRenderHint(QPainter::Antialiasing, false);
    QPen pen;
    pen.setColor(color);
    pen.setWidthF(0);
    painter->setPen(pen);

    const qreal dpr = painter->device()->devicePixelRatio();
    const double pixelStep = 1.0 / dpr;
    const double samplesPerStep = ticksPerPixel * pixelStep * samplesPerTick;
    const int channels = m_audioInfo.channels;
    const qint64 totalFrames = m_audioInfo.frames;

    const int stepCount = static_cast<int>(std::ceil(rectWidth / pixelStep)) + 1;

    const double firstRelativeTick = 0;
    const double lastRelativeTick = rectWidth * ticksPerPixel;
    qint64 sampleStart = static_cast<qint64>(std::floor(firstRelativeTick * samplesPerTick));
    qint64 sampleEnd =
        static_cast<qint64>(std::ceil(lastRelativeTick * samplesPerTick + samplesPerStep));
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
        const double localX = i * pixelStep;
        if (localX > rectWidth)
            break;

        const double tick = rectStartTick + localX * ticksPerPixel;
        const double relativeTick = tick - rectStartTick;
        const double samplePos = relativeTick * samplesPerTick;
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
            if (mono < min)
                min = mono;
            if (mono > max)
                max = mono;
        }

        const double x = rectLeft + localX;
        const double yMin = -logAmplify(min) * halfRectHeight + halfRectHeight + rectTop;
        const double yMax = -logAmplify(max) * halfRectHeight + halfRectHeight + rectTop;
        lines.append(QLineF(x, yMin, x, yMax));
    }

    painter->drawLines(lines);
}
