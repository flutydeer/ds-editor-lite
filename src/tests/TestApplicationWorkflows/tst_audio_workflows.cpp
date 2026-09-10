#include "tst_application_workflows.h"

#include "Modules/Audio/AudioContext.h"
#include "Modules/Audio/AudioExporter.h"
#include "Controller/PlaybackController.h"
#include "Model/AppOptions/AppOptions.h"
#include "../TestSupport/ProcessFixture.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <TalcsCore/AudioBuffer.h>
#include <TalcsCore/MixerAudioSource.h>
#include <TalcsCore/TransportAudioSource.h>
#include <TalcsFormat/AudioFormatIO.h>

#include <QFile>
#include <QDir>
#include <QScopeGuard>
#include <QtTest>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace {
    bool writeAudio(const QString &path, const QVector<float> &samples) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        talcs::AudioFormatIO writer(&file);
        writer.setSampleRate(48000);
        writer.setChannelCount(1);
        writer.setFormat(talcs::AudioFormatIO::WAV | talcs::AudioFormatIO::FLOAT);
        return writer.open(talcs::AbstractAudioFormatIO::Write) &&
               writer.write(samples.constData(), samples.size()) == samples.size();
    }

    Automation::TrackDraftDto audioTrack(const QString &name, const QString &path) {
        Automation::ClipDraftDto clip;
        clip.type = Automation::ClipDraftDto::Type::Audio;
        clip.properties.name = name;
        clip.properties.length = 960;
        clip.properties.clipLen = 960;
        clip.audioPath = path;
        clip.audioInfo.sampleRate = 48000;
        clip.audioInfo.channels = 1;
        clip.audioInfo.frames = 48000;
        Automation::TrackDraftDto track;
        track.name = name;
        track.clips = {clip};
        return track;
    }
}

void ApplicationWorkflowTests::audioExportRespectsRangeMixAndMute() {
    QTemporaryDir files;
    QVERIFY(files.isValid());
    QVector<float> changingSignal(48000, 0.125f);
    std::fill(changingSignal.begin() + 24000, changingSignal.end(), 0.25f);
    const auto firstPath = files.filePath(QStringLiteral("changing.wav"));
    const auto secondPath = files.filePath(QStringLiteral("constant.wav"));
    QVERIFY(writeAudio(firstPath, changingSignal));
    QVERIFY(writeAudio(secondPath, QVector<float>(48000, 0.0625f)));
    auto document = Automation::DocumentAutomationFacade::newDocumentDraft(false);
    document.tracks = {audioTrack(QStringLiteral("Changing"), firstPath),
                       audioTrack(QStringLiteral("Constant"), secondPath)};
    QVERIFY(runtime().documents().commitNewDocument(commandContext(), document));
    QVERIFY(runtime().timeline().setTempo(commandContext(), 0, 120));

    Automation::AudioExportConfigDto config;
    config.fileDirectory = files.path();
    config.sampleRate = 48000;
    config.mono = true;
    config.timeRange = Audio::AudioExporterConfig::TR_All;
    config.sourceOption = Audio::AudioExporterConfig::SO_Custom;
    config.sources = {0};

    const auto exportSamples = [&](const QString &name, QVector<float> &samples,
                                   talcs::AudioFormatIO::MajorFormat expectedFormat) {
        config.fileName = name;
        const auto accepted = runtime().audioExports().start(commandContext(), config, {});
        QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
        const auto terminal = [&] {
            const auto task = runtime().tasks().getTask(accepted.get().document.documentId,
                                                        accepted.get().taskId);
            return task && (task.get().state == Automation::AutomationTaskState::Succeeded ||
                            task.get().state == Automation::AutomationTaskState::Failed ||
                            task.get().state == Automation::AutomationTaskState::Canceled);
        };
        QTRY_VERIFY_WITH_TIMEOUT(terminal(), 10000);
        const auto task =
            runtime().tasks().getTask(accepted.get().document.documentId, accepted.get().taskId);
        QVERIFY(task);
        QVERIFY2(task.get().state == Automation::AutomationTaskState::Succeeded,
                 qPrintable(task.get().error ? task.get().error->message : QString{}));
        QFile file(files.filePath(name));
        QVERIFY(file.open(QIODevice::ReadOnly));
        talcs::AudioFormatIO decoder(&file);
        QVERIFY2(decoder.open(talcs::AbstractAudioFormatIO::Read),
                 qPrintable(decoder.errorString()));
        QCOMPARE(decoder.majorFormat(), expectedFormat);
        QCOMPARE(decoder.sampleRate(), 48000.0);
        QCOMPARE(decoder.channelCount(), 1);
        QCOMPARE(decoder.length(), qint64{48000});
        samples.resize(decoder.length());
        QCOMPARE(decoder.read(samples.data(), samples.size()), qint64(samples.size()));
        QVERIFY(std::all_of(samples.cbegin(), samples.cend(),
                            [](float sample) { return std::isfinite(sample); }));
    };

    QVector<float> selected;
    exportSamples(QStringLiteral("selected.wav"), selected, talcs::AudioFormatIO::WAV);
    if (QTest::currentTestFailed())
        return;
    const auto middle = selected.size() * 3 / 4;
    QVERIFY(selected.at(middle) > 0.1f);
    QVERIFY(std::abs(selected.at(middle) - selected.at(middle + 1000)) < 1e-6f);
    QVERIFY(std::abs(selected.at(middle) / selected.at(selected.size() / 4) - 2.0f) < 1e-5f);

    config.sourceOption = Audio::AudioExporterConfig::SO_All;
    QVector<float> mixed;
    exportSamples(QStringLiteral("mixed.wav"), mixed, talcs::AudioFormatIO::WAV);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(std::abs(mixed.at(middle) / selected.at(middle) - 1.25f) < 1e-5f);

    const auto secondTrack = Automation::TrackId(context->m_appModel->tracks().at(1)->id());
    QVERIFY(runtime().project().setTrackMute(commandContext(), secondTrack, true));
    QVector<float> muted;
    exportSamples(QStringLiteral("muted.wav"), muted, talcs::AudioFormatIO::WAV);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(muted, selected);

    config.fileType = Audio::AudioExporterConfig::FT_Flac;
    QVector<float> compressed;
    exportSamples(QStringLiteral("muted.flac"), compressed, talcs::AudioFormatIO::FLAC);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(compressed.size(), muted.size());
    for (qsizetype i = 0; i < compressed.size(); ++i)
        QVERIFY(std::abs(compressed.at(i) - muted.at(i)) <= 1.0f / 8388608.0f);
}

void ApplicationWorkflowTests::controlledPlaybackLoopsAndBuffers() {
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto path = files.filePath(QStringLiteral("playback.wav"));
    QVERIFY(writeAudio(path, QVector<float>(48000, 0.125f)));
    auto document = Automation::DocumentAutomationFacade::newDocumentDraft(false);
    document.tracks = {audioTrack(QStringLiteral("Playback"), path)};
    QVERIFY(runtime().documents().commitNewDocument(commandContext(), document));
    QVERIFY(runtime().timeline().setTempo(commandContext(), 0, 120));
    auto *audio = AudioContext::instance();
    auto *transport = audio->transport();
    const auto readAheadSize = audio->bufferingReadAheadSize();
    audio->setBufferingReadAheadSize(0);
    QVERIFY(audio->preMixer()->open(256, 48000));
    // The test supplies the audio callback; the external device prerequisite is replaced.
    playbackController->setPlaybackStartGuard([] { return true; });
    bool bufferHeld = false;
    const auto restore = qScopeGuard([&] {
        if (bufferHeld)
            transport->releaseBuffering();
        playbackController->stop();
        transport->pause();
        audio->preMixer()->close();
        audio->setBufferingReadAheadSize(readAheadSize);
        playbackController->setPlaybackStartGuard([] { return false; });
    });
    QVERIFY(runtime().playback().setLoop(commandContext(), LoopSettings(true, 480, 240)));
    QCOMPARE(transport->loopingRange(), qMakePair(qint64{24000}, qint64{36000}));
    QVERIFY(runtime().playback().setPosition(commandContext(), 719));
    QVERIFY(runtime().playback().play(commandContext()));
    QCOMPARE(playbackController->playbackStatus(), PlaybackGlobal::Playing);
    talcs::AudioBuffer buffer(2, 256);
    QTRY_COMPARE_WITH_TIMEOUT(transport->bufferingCounter(), 0, 5000);
    QCOMPARE(audio->preMixer()->read(&buffer), qint64{256});
    QCoreApplication::processEvents();
    QCOMPARE(transport->position(), qint64{24206});
    QCOMPARE(playbackController->position(), 484.12);
    QVERIFY(buffer.constSampleAt(0, 128) > 0);

    transport->acquireBuffering();
    bufferHeld = true;
    const auto heldPosition = transport->position();
    QCOMPARE(audio->preMixer()->read(&buffer), qint64{256});
    QCOMPARE(transport->position(), heldPosition);
    for (int channel = 0; channel < buffer.channelCount(); ++channel)
        for (qint64 sample = 0; sample < buffer.sampleCount(); ++sample)
            QCOMPARE(buffer.constSampleAt(channel, sample), 0.0f);
    transport->releaseBuffering();
    bufferHeld = false;
    QCOMPARE(audio->preMixer()->read(&buffer), qint64{256});
    QCoreApplication::processEvents();
    QCOMPARE(transport->position(), heldPosition + 256);
    QVERIFY(buffer.constSampleAt(0, 128) > 0);
    QVERIFY(runtime().playback().pause(commandContext()));
    QCOMPARE(audio->preMixer()->read(&buffer), qint64{256});
    const auto pausedPosition = transport->position();
    QCOMPARE(audio->preMixer()->read(&buffer), qint64{256});
    QCOMPARE(transport->position(), pausedPosition);
}

void ApplicationWorkflowTests::cancelingAudioExportPreservesExistingFilesAndMixer() {
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto sourcePath = files.filePath(QStringLiteral("source.wav"));
    QVERIFY(writeAudio(sourcePath, QVector<float>(48000, 0.125f)));
    auto document = Automation::DocumentAutomationFacade::newDocumentDraft(false);
    document.tracks = {audioTrack(QStringLiteral("Cancelable render"), sourcePath)};
    QVERIFY(runtime().documents().commitNewDocument(commandContext(), document));
    QVERIFY(runtime().timeline().setTempo(commandContext(), 0, 120));
    const auto before = runtime().documentVersion();
    const auto beforeModel = context->m_appModel->serialize();
    const auto outputPath = files.filePath(QStringLiteral("delivery.wav"));
    const QByteArray originalContents("Previously published audio");
    {
        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::WriteOnly));
        QCOMPARE(output.write(originalContents), qint64(originalContents.size()));
    }

    auto *mixer = AudioContext::instance()->preMixer();
    QVERIFY(mixer->open(512, 48000));
    const auto closeMixer = qScopeGuard([mixer] { mixer->close(); });
    AudioExporter exporter(nullptr);
    AudioExporterConfig config;
    config.setFileDirectory(files.path());
    config.setFileName(QStringLiteral("delivery.wav"));
    config.setFormatSampleRate(44100);
    config.setFormatMono(true);
    exporter.setConfig(config);
    QVERIFY(exporter.warning().testFlag(AudioExporter::W_WillOverwrite));
    bool cancelRequested = false;
    bool temporaryOutputObserved = false;
    connect(&exporter, &AudioExporter::progressChanged, &exporter, [&](const double progress, int) {
        if (cancelRequested || progress <= 0.0 || progress >= 1.0)
            return;
        temporaryOutputObserved =
            !QDir(files.path())
                 .entryList({QStringLiteral("*.exporting")}, QDir::Files | QDir::Hidden)
                 .isEmpty();
        cancelRequested = true;
        exporter.cancel();
    });
    const auto cleanup = qScopeGuard([&] { exporter.cleanUp(); });
    const auto result = exporter.exec();
    QVERIFY(cancelRequested);
    QVERIFY(temporaryOutputObserved);
    QCOMPARE(result, AudioExporter::R_Abort);
    QFile output(outputPath);
    QVERIFY(output.open(QIODevice::ReadOnly));
    QCOMPARE(output.readAll(), originalContents);
    QVERIFY(QDir(files.path())
                .entryList({QStringLiteral("*.exporting")}, QDir::Files | QDir::Hidden)
                .isEmpty());
    QVERIFY(mixer->isOpen());
    QCOMPARE(mixer->bufferSize(), qint64{512});
    QCOMPARE(mixer->sampleRate(), 48000.0);
    QCOMPARE(runtime().documentVersion(), before);
    QCOMPARE(context->m_appModel->serialize(), beforeModel);
}

void ApplicationWorkflowTests::offlineExportRestoresMixerState_data() {
    QTest::addColumn<bool>("initiallyOpen");
    QTest::newRow("closed-mixer") << false;
    QTest::newRow("open-mixer") << true;
}

void ApplicationWorkflowTests::offlineExportRestoresMixerState() {
    QFETCH(bool, initiallyOpen);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto source = files.filePath(QStringLiteral("source.wav"));
    QVERIFY(TestSupport::ProcessFixture::writeWaveFixture(source));
    auto document = Automation::DocumentAutomationFacade::newDocumentDraft(false);
    Automation::ClipDraftDto audio;
    audio.type = Automation::ClipDraftDto::Type::Audio;
    audio.properties.name = QStringLiteral("Audio");
    audio.properties.length = 96;
    audio.properties.clipLen = 96;
    audio.audioPath = source;
    audio.audioInfo.sampleRate = 8000;
    audio.audioInfo.channels = 1;
    audio.audioInfo.frames = 800;
    Automation::TrackDraftDto track;
    track.name = QStringLiteral("Audio");
    track.clips = {audio};
    document.tracks = {track};
    QVERIFY(runtime().documents().commitNewDocument(commandContext(), document));
    auto *mixer = AudioContext::instance()->preMixer();
    mixer->close();
    const auto closeMixer = qScopeGuard([mixer] { mixer->close(); });
    if (initiallyOpen)
        QVERIFY(mixer->open(512, 48000));
    QCOMPARE(mixer->isOpen(), initiallyOpen);

    Automation::AudioExportConfigDto config;
    config.fileName = QStringLiteral("render.wav");
    config.fileDirectory = files.path();
    config.sampleRate = 44100;
    config.mono = true;
    const auto accepted = runtime().audioExports().start(commandContext(), config, {});
    QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
    const auto terminal = [&] {
        const auto task =
            runtime().tasks().getTask(accepted.get().document.documentId, accepted.get().taskId);
        if (!task)
            return false;
        const auto state = task.get().state;
        return state == Automation::AutomationTaskState::Succeeded ||
               state == Automation::AutomationTaskState::Failed ||
               state == Automation::AutomationTaskState::Canceled;
    };
    QTRY_VERIFY_WITH_TIMEOUT(terminal(), 10000);
    const auto task =
        runtime().tasks().getTask(accepted.get().document.documentId, accepted.get().taskId);
    QVERIFY(task);
    QVERIFY2(task.get().state == Automation::AutomationTaskState::Succeeded,
             qPrintable(task.get().error ? task.get().error->message
                                         : QStringLiteral("Audio export did not succeed")));
    QCOMPARE(mixer->isOpen(), initiallyOpen);
    QCOMPARE(mixer->bufferSize(), initiallyOpen ? qint64{512} : qint64{0});
    QCOMPARE(mixer->sampleRate(), initiallyOpen ? 48000.0 : 0.0);
    QFile output(files.filePath(QStringLiteral("render.wav")));
    QVERIFY(output.open(QIODevice::ReadOnly));
    talcs::AudioFormatIO decoder(&output);
    QVERIFY2(decoder.open(talcs::AbstractAudioFormatIO::Read), qPrintable(decoder.errorString()));
    QCOMPARE(decoder.sampleRate(), 44100.0);
    QCOMPARE(decoder.channelCount(), 1);
    QVERIFY(decoder.length() > 0);
}
