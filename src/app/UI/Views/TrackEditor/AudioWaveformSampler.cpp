#include "AudioWaveformSampler.h"

#include "Global/AppGlobal.h"
#include "Modules/Audio/AudioContext.h"

#include <TalcsFormat/AbstractAudioFormatIO.h>
#include <TalcsFormat/FormatManager.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
    constexpr int kSincHalfKernel = 16;
    constexpr double kCurveThreshold = 4.0;
    constexpr int kCurveOversample = 3;
}

AudioWaveformSampler::~AudioWaveformSampler() {
    resetIO();
}

void AudioWaveformSampler::setPath(const QString &path) {
    if (m_path == path)
        return;
    resetIO();
    m_path = path;
    invalidate();
}

void AudioWaveformSampler::invalidate() {
    m_cacheKey.reset();
    m_cachedResult = {};
}

AudioWaveformSampler::Result AudioWaveformSampler::sample(const Request &request) {
    if (!request.audioInfo || !request.timeline || request.previewSceneRect.isEmpty() ||
        request.visibleSceneRect.isEmpty() || request.horizontalScale <= 0.0 ||
        request.pixelsPerQuarterNote <= 0.0 || request.devicePixelRatio <= 0.0 ||
        request.audioInfo->sampleRate <= 0 || request.audioInfo->chunkSize <= 0) {
        invalidate();
        return {};
    }

    const auto cacheKey = makeCacheKey(request);
    if (m_cacheKey == cacheKey)
        return m_cachedResult;

    const auto ticksPerScenePixel =
        AppGlobal::ticksPerQuarterNote / (request.horizontalScale * request.pixelsPerQuarterNote);
    const auto samplesPerTick = static_cast<double>(request.audioInfo->sampleRate) * 60.0 /
                                request.timeline->tempoAt(request.visibleStartTick) /
                                AppGlobal::ticksPerQuarterNote;
    const auto samplesPerDevicePixel =
        ticksPerScenePixel * samplesPerTick / request.devicePixelRatio;

    Result result;
    if (samplesPerDevicePixel > request.audioInfo->chunkSize)
        result = samplePeakMode(request);
    else if (samplesPerDevicePixel > kCurveThreshold)
        result = sampleSubChunkPeakMode(request);
    else
        result = sampleCurveMode(request);
    m_cacheKey = cacheKey;
    m_cachedResult = result;
    return result;
}

AudioWaveformSampler::CacheKey AudioWaveformSampler::makeCacheKey(const Request &request) {
    const auto &info = *request.audioInfo;
    return {
        .audioInfo = request.audioInfo,
        .timeline = request.timeline,
        .peakCacheData = info.peakCache.constData(),
        .peakCacheMipmapData = info.peakCacheMipmap.constData(),
        .materialStartTick = request.materialStartTick,
        .visibleStartTick = request.visibleStartTick,
        .chunkSize = info.chunkSize,
        .mipmapScale = info.mipmapScale,
        .sampleRate = info.sampleRate,
        .channels = info.channels,
        .frames = info.frames,
        .peakCacheSize = info.peakCache.size(),
        .peakCacheMipmapSize = info.peakCacheMipmap.size(),
        .previewSceneRect = request.previewSceneRect,
        .visibleSceneRect = request.visibleSceneRect,
        .horizontalScale = request.horizontalScale,
        .pixelsPerQuarterNote = request.pixelsPerQuarterNote,
        .leftMarginPx = request.leftMarginPx,
        .devicePixelRatio = request.devicePixelRatio,
    };
}

bool AudioWaveformSampler::ensureIO() {
    if (m_io)
        return true;
    if (m_path.isEmpty())
        return false;
    const auto formatManager = AudioContext::instance()->formatManager();
    if (!formatManager)
        return false;
    m_io = formatManager->getFormatLoad(m_path);
    if (!m_io)
        return false;
    if (!m_io->open(talcs::AbstractAudioFormatIO::Read)) {
        delete m_io;
        m_io = nullptr;
        return false;
    }
    return true;
}

void AudioWaveformSampler::resetIO() {
    if (!m_io)
        return;
    m_io->close();
    delete m_io;
    m_io = nullptr;
}

double AudioWaveformSampler::tickToSamplePos(const Request &request, const double tick) const {
    return (request.timeline->tickToMs(tick) -
            request.timeline->tickToMs(request.materialStartTick)) *
           request.audioInfo->sampleRate / 1000.0;
}

double AudioWaveformSampler::samplePosToTick(const Request &request, const double samplePos) const {
    return request.timeline->msToTick(request.timeline->tickToMs(request.materialStartTick) +
                                      samplePos * 1000.0 / request.audioInfo->sampleRate);
}

AudioWaveformSampler::Result AudioWaveformSampler::samplePeakMode(const Request &request) const {
    const auto &info = *request.audioInfo;
    if (info.peakCache.isEmpty() || info.peakCacheMipmap.isEmpty())
        return {};

    const auto useHighResolution = request.horizontalScale >= 0.2;
    const auto &peakData = useHighResolution ? info.peakCache : info.peakCacheMipmap;
    const auto framesPerChunk = useHighResolution
                                    ? static_cast<double>(info.chunkSize)
                                    : static_cast<double>(info.chunkSize) * info.mipmapScale;
    const auto drawLeftScene =
        std::max(request.visibleSceneRect.left(), request.previewSceneRect.left());
    const auto drawRightScene =
        std::min(request.visibleSceneRect.right(), request.previewSceneRect.right());
    if (drawLeftScene >= drawRightScene)
        return {};

    const auto ticksPerScenePixel =
        AppGlobal::ticksPerQuarterNote / (request.horizontalScale * request.pixelsPerQuarterNote);
    const auto deviceScale = request.devicePixelRatio;
    const auto drawLeftLocal = drawLeftScene - request.previewSceneRect.left();
    const auto drawRightLocal = drawRightScene - request.previewSceneRect.left();
    const auto deviceStart = static_cast<int>(std::floor(drawLeftLocal * deviceScale));
    const auto deviceEnd = static_cast<int>(std::ceil(drawRightLocal * deviceScale));

    Result result;
    result.geometry = Geometry::FilledPeaks;
    result.peaks.reserve(deviceEnd - deviceStart + 1);
    const auto halfHeight = request.previewSceneRect.height() * 0.5;
    const auto centerY = request.previewSceneRect.top() + halfHeight;
    const auto peakCount = peakData.size();
    auto previousChunkPos =
        tickToSamplePos(request, (request.previewSceneRect.left() + deviceStart / deviceScale -
                                  request.leftMarginPx) *
                                     ticksPerScenePixel) /
        framesPerChunk;

    for (auto deviceX = deviceStart; deviceX <= deviceEnd; ++deviceX) {
        const auto localX = deviceX / deviceScale;
        const auto nextChunkPos =
            tickToSamplePos(request, (request.previewSceneRect.left() +
                                      (deviceX + 1) / deviceScale - request.leftMarginPx) *
                                         ticksPerScenePixel) /
            framesPerChunk;
        const auto chunkPos = previousChunkPos;
        previousChunkPos = nextChunkPos;
        if (localX < 0.0 || localX > request.previewSceneRect.width())
            continue;

        short minimum = 0;
        short maximum = 0;
        const auto firstChunk = static_cast<int>(std::floor(chunkPos));
        const auto lastChunk = std::max(firstChunk + 1, static_cast<int>(std::ceil(nextChunkPos)));
        for (auto index = firstChunk; index < lastChunk; ++index) {
            if (index < 0)
                continue;
            if (index >= peakCount)
                break;
            const auto &[frameMinimum, frameMaximum] = peakData.at(index);
            minimum = std::min(minimum, frameMinimum);
            maximum = std::max(maximum, frameMaximum);
        }

        result.peaks.append({request.previewSceneRect.left() + localX,
                             centerY - minimum * halfHeight / 32767.0,
                             centerY - maximum * halfHeight / 32767.0});
    }
    return result;
}

AudioWaveformSampler::Result AudioWaveformSampler::sampleSubChunkPeakMode(const Request &request) {
    if (!ensureIO())
        return {};

    const auto drawLeftScene =
        std::max(request.visibleSceneRect.left(), request.previewSceneRect.left());
    const auto drawRightScene =
        std::min(request.visibleSceneRect.right(), request.previewSceneRect.right());
    if (drawLeftScene >= drawRightScene)
        return {};

    const auto &info = *request.audioInfo;
    const auto ticksPerScenePixel =
        AppGlobal::ticksPerQuarterNote / (request.horizontalScale * request.pixelsPerQuarterNote);
    const auto deviceScale = request.devicePixelRatio;
    const auto drawLeftLocal = drawLeftScene - request.previewSceneRect.left();
    const auto drawRightLocal = drawRightScene - request.previewSceneRect.left();
    const auto deviceStart = static_cast<int>(std::floor(drawLeftLocal * deviceScale));
    const auto deviceEnd = static_cast<int>(std::ceil(drawRightLocal * deviceScale));

    auto sampleStart = static_cast<qint64>(
        std::floor(tickToSamplePos(request, (request.previewSceneRect.left() +
                                             deviceStart / deviceScale - request.leftMarginPx) *
                                                ticksPerScenePixel)));
    auto sampleEnd = static_cast<qint64>(
        std::ceil(tickToSamplePos(request, (request.previewSceneRect.left() +
                                            (deviceEnd + 1) / deviceScale - request.leftMarginPx) *
                                               ticksPerScenePixel)));
    sampleStart = std::max(sampleStart, qint64(0));
    sampleEnd = std::min(sampleEnd, info.frames);
    if (sampleEnd <= sampleStart || info.channels <= 0)
        return {};

    const auto framesToRead = sampleEnd - sampleStart;
    m_ioBuffer.resize(static_cast<int>(framesToRead * info.channels));
    m_io->seek(sampleStart);
    const auto framesRead = m_io->read(m_ioBuffer.data(), framesToRead);
    if (framesRead <= 0)
        return {};

    Result result;
    result.geometry = Geometry::VerticalPeaks;
    result.peaks.reserve(deviceEnd - deviceStart + 1);
    const auto halfHeight = request.previewSceneRect.height() * 0.5;
    const auto centerY = request.previewSceneRect.top() + halfHeight;
    auto previousSamplePos =
        tickToSamplePos(request, (request.previewSceneRect.left() + deviceStart / deviceScale -
                                  request.leftMarginPx) *
                                     ticksPerScenePixel);

    for (auto deviceX = deviceStart; deviceX <= deviceEnd; ++deviceX) {
        const auto localX = deviceX / deviceScale;
        const auto nextSamplePos =
            tickToSamplePos(request, (request.previewSceneRect.left() +
                                      (deviceX + 1) / deviceScale - request.leftMarginPx) *
                                         ticksPerScenePixel);
        const auto samplePos = previousSamplePos;
        previousSamplePos = nextSamplePos;
        if (localX < 0.0 || localX > request.previewSceneRect.width())
            continue;

        auto firstFrame = static_cast<qint64>(std::floor(samplePos));
        auto lastFrame = static_cast<qint64>(std::ceil(nextSamplePos));
        firstFrame = std::max(firstFrame, sampleStart);
        lastFrame = std::min(lastFrame, sampleStart + framesRead);
        float minimum = 0.0f;
        float maximum = 0.0f;
        for (auto frame = firstFrame; frame < lastFrame; ++frame) {
            const auto offset = static_cast<int>((frame - sampleStart) * info.channels);
            auto mono = 0.0f;
            for (auto channel = 0; channel < info.channels; ++channel)
                mono += m_ioBuffer[offset + channel];
            mono /= info.channels;
            minimum = std::min(minimum, mono);
            maximum = std::max(maximum, mono);
        }
        result.peaks.append({request.previewSceneRect.left() + localX,
                             centerY - minimum * halfHeight, centerY - maximum * halfHeight});
    }
    return result;
}

AudioWaveformSampler::Result AudioWaveformSampler::sampleCurveMode(const Request &request) {
    if (!ensureIO())
        return {};

    const auto drawLeftScene =
        std::max(request.visibleSceneRect.left(), request.previewSceneRect.left());
    const auto drawRightScene =
        std::min(request.visibleSceneRect.right(), request.previewSceneRect.right());
    if (drawLeftScene >= drawRightScene)
        return {};

    const auto &info = *request.audioInfo;
    if (info.channels <= 0)
        return {};
    const auto ticksPerScenePixel =
        AppGlobal::ticksPerQuarterNote / (request.horizontalScale * request.pixelsPerQuarterNote);
    const auto deviceScale = request.devicePixelRatio;
    const auto drawLeftLocal = drawLeftScene - request.previewSceneRect.left();
    const auto drawRightLocal = drawRightScene - request.previewSceneRect.left();
    const auto deviceStart = static_cast<int>(std::floor(drawLeftLocal * deviceScale));
    const auto deviceEnd = static_cast<int>(std::ceil(drawRightLocal * deviceScale));
    const auto firstSamplePos =
        tickToSamplePos(request, (request.previewSceneRect.left() + deviceStart / deviceScale -
                                  request.leftMarginPx) *
                                     ticksPerScenePixel);
    const auto lastSamplePos =
        tickToSamplePos(request, (request.previewSceneRect.left() + deviceEnd / deviceScale -
                                  request.leftMarginPx) *
                                     ticksPerScenePixel);
    auto sampleStart = static_cast<qint64>(std::floor(firstSamplePos)) - kSincHalfKernel;
    auto sampleEnd = static_cast<qint64>(std::ceil(lastSamplePos)) + kSincHalfKernel;
    sampleStart = std::max(sampleStart, qint64(0));
    sampleEnd = std::min(sampleEnd, info.frames);
    if (sampleEnd <= sampleStart)
        return {};

    const auto framesToRead = sampleEnd - sampleStart;
    m_ioBuffer.resize(static_cast<int>(framesToRead * info.channels));
    m_io->seek(sampleStart);
    const auto framesRead = m_io->read(m_ioBuffer.data(), framesToRead);
    if (framesRead <= 0)
        return {};

    QVector<float> monoSamples(static_cast<int>(framesRead));
    for (auto frame = 0; frame < framesRead; ++frame) {
        auto mono = 0.0f;
        const auto offset = frame * info.channels;
        for (auto channel = 0; channel < info.channels; ++channel)
            mono += m_ioBuffer[offset + channel];
        monoSamples[frame] = mono / info.channels;
    }

    Result result;
    result.geometry = Geometry::Curve;
    const auto pointCount = (deviceEnd - deviceStart) * kCurveOversample + 1;
    result.curve.reserve(pointCount);
    const auto halfHeight = request.previewSceneRect.height() * 0.5;
    const auto centerY = request.previewSceneRect.top() + halfHeight;
    for (auto index = 0; index < pointCount; ++index) {
        const auto deviceX = deviceStart + static_cast<double>(index) / kCurveOversample;
        const auto localX = deviceX / deviceScale;
        if (localX < 0.0 || localX > request.previewSceneRect.width())
            continue;
        const auto sceneX = request.previewSceneRect.left() + localX;
        const auto samplePos =
            tickToSamplePos(request, (sceneX - request.leftMarginPx) * ticksPerScenePixel);
        const auto value = sincInterpolate(monoSamples, sampleStart, info.frames, samplePos);
        result.curve.append({sceneX, centerY - value * halfHeight});
    }

    const auto samplesPerLogicalPixel =
        deviceEnd > deviceStart
            ? (lastSamplePos - firstSamplePos) / ((deviceEnd - deviceStart) / deviceScale)
            : 0.0;
    if (samplesPerLogicalPixel <= 0.0 || samplesPerLogicalPixel >= 1.0 / 6.0)
        return result;

    result.sampleDotRadius = std::min(3.0, 1.0 / samplesPerLogicalPixel * 0.3);
    const auto firstSample = static_cast<qint64>(std::floor(firstSamplePos));
    const auto lastSample = static_cast<qint64>(std::ceil(lastSamplePos));
    for (auto sampleIndex = std::max(firstSample, qint64(0));
         sampleIndex < std::min(lastSample, info.frames); ++sampleIndex) {
        const auto bufferIndex = static_cast<int>(sampleIndex - sampleStart);
        if (bufferIndex < 0 || bufferIndex >= monoSamples.size())
            continue;
        const auto sceneX =
            samplePosToTick(request, static_cast<double>(sampleIndex)) / ticksPerScenePixel +
            request.leftMarginPx;
        if (sceneX < request.previewSceneRect.left() || sceneX > request.previewSceneRect.right()) {
            continue;
        }
        result.sampleDots.append({sceneX, centerY - monoSamples[bufferIndex] * halfHeight});
    }
    return result;
}

double AudioWaveformSampler::sincInterpolate(const QVector<float> &samples, const qint64 offset,
                                             const qint64 totalFrames, const double position) {
    const auto center = static_cast<qint64>(std::floor(position));
    const auto fraction = position - center;
    auto result = 0.0;
    constexpr auto pi = std::numbers::pi_v<double>;
    for (auto index = -kSincHalfKernel; index <= kSincHalfKernel; ++index) {
        const auto sampleIndex = center + index;
        if (sampleIndex < 0 || sampleIndex >= totalFrames)
            continue;
        const auto bufferIndex = static_cast<int>(sampleIndex - offset);
        if (bufferIndex < 0 || bufferIndex >= samples.size())
            continue;
        const auto x = fraction - index;
        auto sinc = 1.0;
        auto window = 1.0;
        if (std::abs(x) >= 1e-9) {
            sinc = std::sin(pi * x) / (pi * x);
            const auto windowX = x / kSincHalfKernel;
            window = std::abs(windowX) < 1.0 ? std::sin(pi * windowX) / (pi * windowX) : 0.0;
        }
        result += samples[bufferIndex] * sinc * window;
    }
    return result;
}
