#include "tst_application_gui.h"

#include "UI/Views/ClipEditor/AudioView/AudioDbAxis.h"
#include "UI/Views/ClipEditor/AudioView/AudioViewGraphicsScene.h"
#include "UI/Views/ClipEditor/AudioView/AudioViewGraphicsView.h"
#include "UI/Views/ClipEditor/AudioView/AudioViewRhiWidget.h"
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
#include <QScrollBar>
#include <QSignalSpy>
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
        QVERIFY(std::abs(tick.distanceFromCenter - std::pow(10.0, tick.db / 20.0) * 200.0) < 1e-9);
    }

    QCOMPARE(finiteDbs(AudioDbAxis::buildTicks(100.0, 12)),
             QList<double>({0.0, -3.0, -6.0, -9.0, -15.0}));
    QCOMPARE(finiteDbs(AudioDbAxis::buildTicks(60.0, 12)), QList<double>({0.0, -3.0, -6.0, -12.0}));
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
        bestColumnShare = std::max(bestColumnShare, static_cast<double>(changed) / after.height());
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

// Both backends open a clip at its own left edge. Centring would put the first
// frame in the middle of the clip, and the scene starts at the clip's first
// audible tick rather than at the timeline origin, so the tick readouts and the
// scroll range both describe the clip and a clip that starts later does not open
// on a blank stretch of canvas
void ApplicationGuiTests::audioViewStartsAtTheLeftEnd() {
    AudioViewGraphicsScene scene;
    AudioViewGraphicsView view(&scene);
    view.resize(600, 200);
    view.show();

    AudioViewContent content;
    content.clipId = 1;
    content.start = 1920;
    content.length = 480000;
    content.clipLen = 480000;
    content.loadState = AppGlobal::AudioClipLoadState::Loaded;
    view.setContent(content);
    QTest::qWait(120);

    QCOMPARE(view.horizontalScrollBar()->value(), view.horizontalScrollBar()->minimum());
    QCOMPARE(view.startTick(), static_cast<double>(content.contentStartTick()));

    // The scroll range is the material and nothing else: its far end is the end
    // of the material in global ticks, not the clip length added to the origin
    view.horizontalScrollBar()->setValue(view.horizontalScrollBar()->maximum());
    QTest::qWait(50);
    QVERIFY2(std::abs(view.endTick() - content.materialEndTick()) < 1.0,
             qPrintable(QStringLiteral("endTick=%1 materialEnd=%2")
                            .arg(view.endTick())
                            .arg(content.materialEndTick())));
}

// The RHI canvas is view-only, but a finger still has to pan and pinch it, the same
// way it moves the piano roll and the arrangement
void ApplicationGuiTests::audioViewRhiTakesTouchGestures() {
    AudioViewRhiWidget view;
    view.resize(600, 200);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    AudioViewContent content;
    content.clipId = 1;
    content.length = 480000;
    content.clipLen = 480000;
    content.start = 1920;
    content.loadState = AppGlobal::AudioClipLoadState::Loaded;
    view.setContent(content);
    QTest::qWait(200);

    // The RHI canvas opens on the clip's own left edge too, and its scroll bar
    // cannot leave the material
    QCOMPARE(view.horizontalBarValue(), 0);
    QCOMPARE(view.startTick(), static_cast<double>(content.contentStartTick()));

    // One finger travelling left drags the material along, so the bar leaves the left end
    auto *device = QTest::createTouchDevice();
    auto pan = QTest::touchEvent(&view, device, false);
    pan.press(0, QPoint(500, 100)).commit(false);
    pan.move(0, QPoint(400, 100)).commit(false);
    pan.move(0, QPoint(300, 100)).commit(false);
    pan.release(0, QPoint(300, 100)).commit(false);
    QTest::qWait(80);
    QVERIFY2(view.horizontalBarValue() > 0,
             qPrintable(QStringLiteral("bar=%1").arg(view.horizontalBarValue())));

    // Two fingers spreading apart widen the time axis
    const auto before = view.scaleX();
    auto pinch = QTest::touchEvent(&view, device, false);
    pinch.press(0, QPoint(250, 100)).commit(false);
    pinch.press(1, QPoint(350, 100)).commit(false);
    pinch.move(0, QPoint(150, 100)).commit(false);
    pinch.move(1, QPoint(450, 100)).commit(false);
    pinch.release(0, QPoint(150, 100)).commit(false);
    pinch.release(1, QPoint(450, 100)).commit(false);
    QTest::qWait(80);
    QVERIFY2(view.scaleX() > before,
             qPrintable(QStringLiteral("before=%1 after=%2").arg(before).arg(view.scaleX())));
}

// Dragging the clip along the timeline only moves the material, it does not change
// which part of it the user was looking at, so both canvases keep their local scroll
// position instead of jumping to another section
void ApplicationGuiTests::audioViewKeepsItsLocalScrollWhenTheClipMoves() {
    AudioViewContent content;
    content.clipId = 1;
    content.start = 1920;
    content.length = 480000;
    content.clipLen = 480000;
    content.loadState = AppGlobal::AudioClipLoadState::Loaded;

    AudioViewGraphicsScene scene;
    AudioViewGraphicsView legacyView(&scene);
    legacyView.resize(600, 200);
    legacyView.show();
    legacyView.setContent(content);
    QTest::qWait(120);
    legacyView.horizontalScrollBar()->setValue(150);
    QTest::qWait(50);
    const auto legacyBar = legacyView.horizontalScrollBar()->value();
    QVERIFY(legacyBar > 0);
    const auto legacyLocalTick = legacyView.startTick() - content.contentStartTick();

    AudioViewRhiWidget rhiView;
    rhiView.resize(600, 200);
    rhiView.show();
    QVERIFY(QTest::qWaitForWindowExposed(&rhiView));
    rhiView.setContent(content);
    QTest::qWait(150);
    rhiView.setHorizontalBarValue(150);
    QTest::qWait(50);
    const auto rhiBar = rhiView.horizontalBarValue();
    QVERIFY(rhiBar > 0);
    const auto rhiLocalTick = rhiView.startTick() - content.contentStartTick();

    // The clip moves two minutes later on the timeline: same clip, same material
    content.start = 96000;
    legacyView.setContent(content);
    rhiView.setContent(content);
    QTest::qWait(120);

    QCOMPARE(legacyView.horizontalScrollBar()->value(), legacyBar);
    QCOMPARE(rhiView.horizontalBarValue(), rhiBar);
    QVERIFY2(std::abs((legacyView.startTick() - content.contentStartTick()) - legacyLocalTick) <
                 1.0,
             qPrintable(QStringLiteral("legacy local tick %1, expected %2")
                            .arg(legacyView.startTick() - content.contentStartTick())
                            .arg(legacyLocalTick)));
    QVERIFY2(std::abs((rhiView.startTick() - content.contentStartTick()) - rhiLocalTick) < 1.0,
             qPrintable(QStringLiteral("rhi local tick %1, expected %2")
                            .arg(rhiView.startTick() - content.contentStartTick())
                            .arg(rhiLocalTick)));
}