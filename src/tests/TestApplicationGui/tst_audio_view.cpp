#include "tst_application_gui.h"

#include "UI/Views/ClipEditor/AudioView/AudioDbAxis.h"
#include "UI/Views/Common/AudioLoadHint.h"
#include "UI/Views/ClipEditor/AudioView/AudioView.h"
#include "UI/Views/TrackEditor/AudioWaveformSampler.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/PlaybackController.h"
#include "Controller/TrackController.h"
#include "Global/ControllerGlobal.h"
#include "Model/AppStatus/AppStatus.h"

#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/Tasking/TaskManager.h>

#include "../TestSupport/MainWindowFixture.h"
#include "../TestSupport/WaveFixture.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <cmath>
#include <numbers>
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

void ApplicationGuiTests::audioViewDrawsThePlaybackPosition() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto &runtime = *context->m_coreRuntime;
    TestSupport::MainWindowFixture host;
    host.show();
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));

    const auto path = dir.filePath(QStringLiteral("stereo.wav"));
    QVector<float> samples;
    for (auto i = 0; i < 96000; i++) {
        const auto phase = 2.0 * std::numbers::pi * 220.0 * i / 48000.0;
        const auto envelope = std::exp(-static_cast<double>(i) / 48000.0);
        samples.append(static_cast<float>(0.95 * envelope * std::sin(phase)));
        samples.append(static_cast<float>(0.3 * envelope * std::sin(phase * 3.0)));
    }
    QVERIFY(TestSupport::writeWave(path, samples, 2, 48000));

    Automation::ClipDraftDto clipDraft;
    clipDraft.type = Automation::ClipDraftDto::Type::Audio;
    clipDraft.properties.name = QStringLiteral("Stereo audio clip");
    clipDraft.properties.start = 0;
    clipDraft.properties.clipStart = 0;
    clipDraft.properties.length = 3840;
    clipDraft.properties.clipLen = 3840;
    clipDraft.properties.trimStartMs = 0;
    clipDraft.properties.playLengthMs = 4000;
    clipDraft.properties.materialLengthMs = 4000;
    clipDraft.audioPath = path;
    clipDraft.hasRealTimeAnchor = true;
    Automation::TrackDraftDto trackDraft;
    trackDraft.name = QStringLiteral("Audio track");
    trackDraft.defaultLanguage = QStringLiteral("eng");
    trackDraft.clips.append(clipDraft);
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, trackDraft));
    const auto *clip = *context->m_appModel->tracks().first()->clips().begin();
    QVERIFY(qobject_cast<const AudioClip *>(clip));
    QTRY_VERIFY(taskManager->tasks().isEmpty());

    trackController->setSelectedClips({clip->id()});
    TrackController::setActiveClip(clip->id());
    QCoreApplication::processEvents();
    QTest::qWait(300);

    auto *audioView = host.window->findChild<AudioView *>();
    QVERIFY(audioView);
    QVERIFY(audioView->isVisible());
    QTRY_VERIFY(audioView->height() > 100);

    const auto before = audioView->grab().toImage();
    // The position travels through the controller, exactly like the piano roll
    playbackController->setPosition(2400.0);
    QCoreApplication::processEvents();
    QTest::qWait(200);
    const auto after = audioView->grab().toImage();
    QCOMPARE(after.size(), before.size());

    // The indicator is a full height line, so at least one column has to differ
    // from the previous frame along almost the whole canvas
    auto bestColumnShare = 0.0;
    for (auto x = 0; x < after.width(); x++) {
        auto changed = 0;
        for (auto y = 0; y < after.height(); y++) {
            if (after.pixel(x, y) != before.pixel(x, y))
                changed++;
        }
        bestColumnShare =
            std::max(bestColumnShare, static_cast<double>(changed) / after.height());
    }
    QVERIFY(bestColumnShare > 0.8);
}

// One place decides what the audio view says about a clip that is not ready, so
// both render backends paint the same hint
void ApplicationGuiTests::audioLoadHintMapsTheLoadStates() {
    const auto loading = AudioLoadHint::textFor(AppGlobal::AudioClipLoadState::Loading);
    const auto missing = AudioLoadHint::textFor(AppGlobal::AudioClipLoadState::Missing);
    QVERIFY(!loading.isEmpty());
    QVERIFY(!missing.isEmpty());
    QVERIFY(loading != missing);
    QVERIFY(AudioLoadHint::textFor(AppGlobal::AudioClipLoadState::Loaded).isEmpty());
    // A resolver that reported a reason appends it to the permanent hint
    QVERIFY(AudioLoadHint::textFor(AppGlobal::AudioClipLoadState::Missing, QStringLiteral("gone"))
                .contains(QStringLiteral("gone")));

    // The missing hint is dimmed so it reads as a failure notice, the loading one is not
    const QColor clipColor(120, 160, 255);
    QCOMPARE(AudioLoadHint::colorFor(AppGlobal::AudioClipLoadState::Loading, clipColor), clipColor);
    QVERIFY(
        qAbs(AudioLoadHint::colorFor(AppGlobal::AudioClipLoadState::Missing, clipColor).alphaF() -
             clipColor.alphaF() * 0.5) < 0.01);
}
