#include "tst_audio_assets.h"

#include "Controller/Tasks/DecodeAudioTask.h"
#include "Modules/Audio/AudioContext.h"
#include "UI/Views/TrackEditor/AudioWaveformSampler.h"

#include <TalcsFormat/AudioFormatIO.h>
#include <TalcsFormat/FormatManager.h>

#include <QFile>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtTest>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
    bool writeWave(const QString &path, const QVector<float> &samples, int channels) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        talcs::AudioFormatIO writer(&file);
        writer.setSampleRate(48000);
        writer.setChannelCount(channels);
        writer.setFormat(talcs::AudioFormatIO::WAV | talcs::AudioFormatIO::FLOAT);
        const auto frames = samples.size() / channels;
        return writer.open(talcs::AbstractAudioFormatIO::Write) &&
               writer.write(samples.constData(), frames) == frames;
    }

    bool decodeWave(const QString &path, AudioInfoModel &info) {
        DecodeAudioTask task;
        task.path = path;
        task.io = AudioContext::instance()->formatManager()->getFormatLoad(path);
        QThreadPool pool;
        pool.start(&task);
        if (!pool.waitForDone(10000) || !task.success)
            return false;
        info = task.result();
        return true;
    }

    QVector<float> stereoWave() {
        QVector<float> samples(48000 * 2);
        for (auto frame = 0; frame < 48000; ++frame) {
            const auto signal =
                static_cast<float>(0.5 * std::sin(2.0 * std::numbers::pi * frame / 32.0));
            samples[frame * 2] = signal + 0.125f;
            samples[frame * 2 + 1] = signal - 0.125f;
        }
        return samples;
    }

    AudioWaveformSampler::Request waveformRequest(const AudioInfoModel &info,
                                                  const Timeline &timeline, double scale) {
        const auto pixelsPerTick = scale * 64.0 / 480.0;
        const auto materialStart = 960;
        const auto materialEnd = timeline.msToTick(timeline.tickToMs(materialStart) +
                                                   info.frames * 1000.0 / info.sampleRate);
        const auto left = 12.0 + materialStart * pixelsPerTick;
        const auto width = (materialEnd - materialStart) * pixelsPerTick;
        return {.audioInfo = &info,
                .timeline = &timeline,
                .materialStartTick = materialStart,
                .visibleStartTick = materialStart,
                .previewSceneRect = QRectF(left, 10.0, width, 80.0),
                .visibleSceneRect = QRectF(left, 10.0, std::min(width, 128.0), 80.0),
                .horizontalScale = scale,
                .leftMarginPx = 12.0};
    }

    void verifyPoint(const QVector<QPointF> &points, const QPointF &expected) {
        const auto found = std::find_if(points.cbegin(), points.cend(), [&](const QPointF &point) {
            return std::abs(point.x() - expected.x()) < 1e-6;
        });
        QVERIFY2(found != points.cend(), "The waveform must retain the requested sample position");
        QVERIFY2(std::abs(found->y() - expected.y()) < 1e-4,
                 qPrintable(QStringLiteral("Sample height: actual %1, expected %2")
                                .arg(found->y())
                                .arg(expected.y())));
    }
}

void AudioAssetsTests::decodedWaveformRetainsPeaks_data() {
    QTest::addColumn<int>("channels");
    QTest::newRow("mono") << 1;
    QTest::newRow("stereo") << 2;
}

void AudioAssetsTests::decodedWaveformRetainsPeaks() {
    QFETCH(int, channels);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    constexpr auto frames = 1283;
    QVector<float> samples(frames * channels, 0.0f);
    const auto setFrame = [&](int frame, float average) {
        for (auto channel = 0; channel < channels; ++channel)
            samples[frame * channels + channel] =
                average + (channels == 1 ? 0.0f : (channel == 0 ? 0.25f : -0.25f));
    };
    setFrame(1278, 0.5f);
    setFrame(1279, -0.5f);
    setFrame(1282, 0.75f);
    const auto path = files.filePath(QStringLiteral("transients.wav"));
    QVERIFY(writeWave(path, samples, channels));
    AudioInfoModel info;
    QVERIFY(decodeWave(path, info));
    QCOMPARE(info.channels, channels);
    QCOMPARE(info.frames, qint64{frames});
    for (const auto *cache : {&info.peakCache, &info.peakCacheMipmap}) {
        QVERIFY(!cache->isEmpty());
        short minimum = 0;
        short maximum = 0;
        for (const auto &[low, high] : *cache) {
            minimum = std::min(minimum, low);
            maximum = std::max(maximum, high);
        }
        QCOMPARE(minimum, short(-0.5 * 32767));
        QCOMPARE(maximum, short(0.75 * 32767));
    }
}

void AudioAssetsTests::waveformSamplingFollowsZoom_data() {
    QTest::addColumn<double>("scale");
    QTest::addColumn<AudioWaveformSampler::Geometry>("geometry");
    QTest::addColumn<bool>("sampleDots");
    using Geometry = AudioWaveformSampler::Geometry;
    QTest::newRow("overview") << 0.1 << Geometry::FilledPeaks << false;
    QTest::newRow("peaks") << 1.0 << Geometry::FilledPeaks << false;
    QTest::newRow("detailed-peaks") << 10.0 << Geometry::VerticalPeaks << false;
    QTest::newRow("curve") << 375.0 << Geometry::Curve << false;
    QTest::newRow("individual-samples") << 3000.0 << Geometry::Curve << true;
}

void AudioAssetsTests::waveformSamplingFollowsZoom() {
    QFETCH(double, scale);
    QFETCH(AudioWaveformSampler::Geometry, geometry);
    QFETCH(bool, sampleDots);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto path = files.filePath(QStringLiteral("stereo.wav"));
    QVERIFY(writeWave(path, stereoWave(), 2));
    AudioInfoModel info;
    QVERIFY(decodeWave(path, info));
    Timeline timeline({
        {0, 120.0}
    });
    const auto request = waveformRequest(info, timeline, scale);
    AudioWaveformSampler sampler;
    sampler.setPath(path);
    const auto result = sampler.sample(request);
    QCOMPARE(result.geometry, geometry);
    if (geometry == AudioWaveformSampler::Geometry::Curve) {
        const auto samplesPerPixel = 375.0 / scale;
        const auto peak = QPointF(request.previewSceneRect.left() + 8.0 / samplesPerPixel, 30.0);
        verifyPoint(result.curve, peak);
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(!result.sampleDots.isEmpty(), sampleDots);
        if (sampleDots) {
            QVERIFY(result.sampleDotRadius > 0.0);
            verifyPoint(result.sampleDots, peak);
        }
    } else {
        QVERIFY(!result.peaks.isEmpty());
        const auto &peak = result.peaks.first();
        QVERIFY(std::abs(peak.yMin - 70.0) < 0.01);
        QVERIFY(std::abs(peak.yMax - 30.0) < 0.01);
    }
}

void AudioAssetsTests::waveformSamplingRefreshesSourceAndTempo() {
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto firstPath = files.filePath(QStringLiteral("periodic.wav"));
    const auto secondPath = files.filePath(QStringLiteral("constant.wav"));
    QVERIFY(writeWave(firstPath, stereoWave(), 2));
    QVERIFY(writeWave(secondPath, QVector<float>(48000 * 2, -0.5f), 2));
    AudioInfoModel info;
    QVERIFY(decodeWave(firstPath, info));
    Timeline timeline({
        {0, 120.0}
    });
    const auto request = waveformRequest(info, timeline, 375.0);
    const auto sampleX = request.previewSceneRect.left() + 8.0;
    AudioWaveformSampler sampler;
    sampler.setPath(firstPath);
    const auto first = sampler.sample(request);
    verifyPoint(first.curve, QPointF(sampleX, 30.0));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(sampler.sample(request).curve, first.curve);

    timeline.setTempos({
        {0, 60.0}
    });
    sampler.invalidate();
    verifyPoint(sampler.sample(request).curve, QPointF(sampleX, 50.0));
    if (QTest::currentTestFailed())
        return;

    sampler.setPath(secondPath);
    verifyPoint(sampler.sample(request).curve, QPointF(sampleX, 70.0));
    if (QTest::currentTestFailed())
        return;
    sampler.setPath(files.filePath(QStringLiteral("missing.wav")));
    const auto missing = sampler.sample(request);
    QCOMPARE(missing.geometry, AudioWaveformSampler::Geometry::None);
    QVERIFY(missing.curve.isEmpty());
}
