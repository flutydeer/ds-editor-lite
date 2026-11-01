#include "tst_application_gui.h"

#include "UI/Views/ClipEditor/AudioView/AudioDbAxis.h"
#include "UI/Views/TrackEditor/AudioWaveformSampler.h"

#include <lite/MusicBase/Timeline.h>

#include <QtTest/QTest>

#include <cmath>

namespace {
    QList<double> finiteDbs(const QVector<AudioDbAxis::Tick> &ticks) {
        QList<double> result;
        for (const auto &tick : ticks) {
            if (!tick.infinite)
                result.append(tick.db);
        }
        return result;
    }

    // Total vertical extent of the tallest peak in a sampled lane. yMax holds the
    // upper edge on screen, so the difference is only positive after abs().
    double peakSpan(const WaveformRenderUtils::SampledWaveform &waveform) {
        auto span = 0.0;
        for (const auto &peak : waveform.peaks)
            span = std::max(span, std::abs(peak.yMax - peak.yMin));
        return span;
    }

    // Two-channel info whose channel lanes differ by a known ratio, plus a
    // downmixed cache that is neither
    AudioInfoModel buildStereoInfo() {
        AudioInfoModel info;
        info.chunkSize = 100;
        info.mipmapScale = 8;
        info.sampleRate = 48000;
        info.channels = 2;
        info.frames = 1000;
        for (auto i = 0; i < 10; i++)
            info.peakCache.append({-2000, 2000});
        info.peakCacheMipmap.append({-2000, 2000});
        QList<std::tuple<short, short>> left;
        QList<std::tuple<short, short>> right;
        for (auto i = 0; i < 10; i++) {
            left.append({-1000, 1000});
            right.append({-8000, 4000});
        }
        info.peakCacheChannels = {left, right};
        info.peakCacheMipmapChannels = {left, right};
        return info;
    }
}

void ApplicationGuiTests::audioDbAxisTickLadderFollowsTheLaneHeight() {
    // A tall lane keeps the 3 dB steps until the labels would collide, then the
    // surviving gaps double, which is the console ruler behaviour
    const auto tall = AudioDbAxis::buildTicks(200.0, 12);
    QCOMPARE(finiteDbs(tall), QList<double>({0.0, -3.0, -6.0, -9.0, -12.0, -15.0, -21.0}));
    QVERIFY(tall.constLast().infinite);
    QCOMPARE(tall.constLast().distanceFromCenter, 0.0);
    QCOMPARE(tall.constFirst().distanceFromCenter, 200.0);

    // Every finite tick sits at its linear amplitude position
    for (const auto &tick : tall) {
        if (tick.infinite)
            continue;
        QVERIFY(std::abs(tick.distanceFromCenter - std::pow(10.0, tick.db / 20.0) * 200.0) <
                1e-9);
    }

    QCOMPARE(finiteDbs(AudioDbAxis::buildTicks(100.0, 12)),
             QList<double>({0.0, -3.0, -6.0, -9.0, -15.0}));
    QCOMPARE(finiteDbs(AudioDbAxis::buildTicks(60.0, 12)),
             QList<double>({0.0, -3.0, -6.0, -12.0}));
    // -3 lands too close to 0 to carry a label, and the next step that fits is
    // -6: the ruler thins out by skipping, never by squeezing
    QCOMPARE(finiteDbs(AudioDbAxis::buildTicks(25.0, 12)), QList<double>({0.0, -6.0}));
    QCOMPARE(finiteDbs(AudioDbAxis::buildTicks(15.0, 12)), QList<double>({0.0}));

    // A lane too short for any label keeps the silence end only, and a lane
    // under the minimum height keeps nothing
    const auto shortLane = AudioDbAxis::buildTicks(12.0, 12);
    QVERIFY(shortLane.constLast().infinite);
    QCOMPARE(finiteDbs(shortLane), QList<double>({0.0}));
    QVERIFY(AudioDbAxis::buildTicks(8.0, 12).isEmpty());

    // Labels use the ASCII digits of the ladder and a real minus sign for silence
    QCOMPARE(AudioDbAxis::tickLabel(tall.constFirst()), QStringLiteral("0"));
    QCOMPARE(AudioDbAxis::tickLabel(tall.at(1)), QStringLiteral("-3"));
    const auto silence = QString(QChar(0x2212)) + QString(QChar(0x221E));
    QCOMPARE(AudioDbAxis::tickLabel(tall.constLast()), silence);
}

void ApplicationGuiTests::audioWaveformSamplerUsesTheRequestedChannelLane() {
    const auto info = buildStereoInfo();
    const Timeline timeline;
    AudioWaveformSampler sampler;

    AudioWaveformSampler::Request request;
    request.audioInfo = &info;
    request.timeline = &timeline;
    request.previewSceneRect = QRectF(0.0, 0.0, 100.0, 64.0);
    request.visibleSceneRect = request.previewSceneRect;
    // Far enough out that the sampler takes the peak path over the raw samples
    request.horizontalScale = 0.2;
    request.pixelsPerQuarterNote = 96.0;

    request.channel = 0;
    const auto left = sampler.sample(request);
    request.channel = 1;
    const auto right = sampler.sample(request);
    QCOMPARE(left.geometry, WaveformRenderUtils::Geometry::FilledPeaks);
    QCOMPARE(right.geometry, WaveformRenderUtils::Geometry::FilledPeaks);
    QVERIFY(!left.peaks.isEmpty());
    QCOMPARE(right.peaks.size(), left.peaks.size());

    // -1000..1000 against -8000..4000 spans six times as much
    const auto leftSpan = peakSpan(left);
    const auto rightSpan = peakSpan(right);
    QVERIFY(leftSpan > 0.0);
    QVERIFY2(std::abs(rightSpan / leftSpan - 6.0) < 0.01,
             qPrintable(QStringLiteral("lanes must be sampled independently, got ratio %1")
                            .arg(rightSpan / leftSpan)));

    // The left lane is symmetric about the lane centre, the right one is not
    const auto centerY = request.previewSceneRect.center().y();
    QVERIFY(std::abs((left.peaks.constFirst().yMin + left.peaks.constFirst().yMax) * 0.5 -
                     centerY) < 1e-9);
    QVERIFY(std::abs((right.peaks.constFirst().yMin + right.peaks.constFirst().yMax) * 0.5 -
                     centerY) > 0.5);

    // A lane index without its own cache falls back to the downmixed one
    request.channel = 2;
    const auto fallback = sampler.sample(request);
    QVERIFY2(std::abs(peakSpan(fallback) / leftSpan - 2.0) < 0.01,
             "a missing channel cache must fall back to the downmixed cache");

    // The downmixed behaviour the track editor relies on is unchanged
    request.channel = -1;
    QVERIFY(std::abs(peakSpan(sampler.sample(request)) / leftSpan - 2.0) < 0.01);
}
