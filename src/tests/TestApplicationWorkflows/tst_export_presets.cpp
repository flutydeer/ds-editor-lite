#include "tst_application_workflows.h"

#include "Automation/AppOptionsAutomationAdapter.h"
#include "Automation/AudioExportAutomationAdapter.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Audio/AudioExporter.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/Tasking/TaskManager.h>
#include <TalcsFormat/AudioFormatIO.h>

#include <QFile>
#include <QScopeGuard>
#include <QtTest>

#include <algorithm>
#include <cmath>

void ApplicationWorkflowTests::customExportPresetPersistsAndProducesIntegerWave() {
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto original = runtime().settings().getSettings();
    QVERIFY(original);
    const auto restoreSettings =
        qScopeGuard([&] { QVERIFY(runtime().settings().updateAudio({}, original.get().audio)); });

    const auto sourcePath = files.filePath(QStringLiteral("source.wav"));
    {
        QFile source(sourcePath);
        QVERIFY(source.open(QIODevice::WriteOnly));
        talcs::AudioFormatIO writer(&source);
        writer.setSampleRate(48000);
        writer.setChannelCount(1);
        writer.setFormat(talcs::AudioFormatIO::WAV | talcs::AudioFormatIO::FLOAT);
        QVERIFY(writer.open(talcs::AbstractAudioFormatIO::Write));
        const QVector<float> samples(4800, 0.125f);
        QCOMPARE(writer.write(samples.constData(), samples.size()), qint64{4800});
    }
    Automation::ClipDraftDto audio;
    audio.type = Automation::ClipDraftDto::Type::Audio;
    audio.properties.name = QStringLiteral("Preset source");
    audio.properties.length = 96;
    audio.properties.clipLen = 96;
    audio.audioPath = sourcePath;
    Automation::TrackDraftDto track;
    track.name = QStringLiteral("Audio");
    track.clips = {audio};
    auto document = Automation::DocumentAutomationFacade::newDocumentDraft(false);
    document.tracks = {track};
    QVERIFY(runtime().documents().commitNewDocument(commandContext(), document));
    QVERIFY(runtime().timeline().setTempo(commandContext(), 0, 120));
    auto *imported =
        dynamic_cast<AudioClip *>(*context->m_appModel->tracks().first()->clips().begin());
    QVERIFY(imported);
    QTRY_VERIFY_WITH_TIMEOUT(
        imported->audioInfo().frames == 4800 && !imported->audioInfo().peakCache.isEmpty(), 10000);
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    const auto before = runtime().documentVersion();
    const auto beforeModel = context->m_appModel->serialize();
    const auto *beforeUndo = HistoryManager::instance()->nextUndoEntry();

    const auto name = QStringLiteral("Test integer-wave delivery");
    QVERIFY(!AudioExporter::presets().contains(name));
    AudioExporterConfig draft;
    draft.setFileDirectory(files.path());
    draft.setFileName(QStringLiteral("draft.wav"));
    draft.setFileType(AudioExporterConfig::FT_Wav);
    draft.setFormatOption(0);
    draft.setFormatMono(false);
    draft.setFormatSampleRate(48000);
    AudioExporter::addPreset(name, draft);
    QCOMPARE(AudioExporter::presets().count(name), 1);
    QCOMPARE(Automation::toAutomationDto(AudioExporter::preset(name)),
             Automation::toAutomationDto(draft));

    auto updated = draft;
    updated.setFileName(QStringLiteral("delivery_${sampleRate}.wav"));
    updated.setFormatOption(2);
    updated.setFormatMono(true);
    updated.setFormatSampleRate(44100);
    QCOMPARE(draft.fileName(), QStringLiteral("draft.wav"));
    AudioExporter::addPreset(name, updated);
    QCOMPARE(AudioExporter::presets().count(name), 1);
    QCOMPARE(Automation::toAutomationDto(AudioExporter::preset(name)),
             Automation::toAutomationDto(updated));

    Automation::AudioExportConfigDto reloadedConfig;
    {
        AppOptions reopened;
        const auto persisted = Automation::createAppOptionsAutomationServices(&reopened).snapshot();
        const auto &presets = persisted.audio.audioExporterPresets;
        const auto found = std::find_if(presets.cbegin(), presets.cend(),
                                        [&](const auto &preset) { return preset.name == name; });
        QVERIFY(found != presets.cend());
        reloadedConfig = found->config;
        QCOMPARE(reloadedConfig, Automation::toAutomationDto(updated));
    }
    const auto preview = runtime().audioExports().preview(before.documentId, reloadedConfig);
    QVERIFY(preview);
    const auto outputPath = files.filePath(QStringLiteral("delivery_44100.wav"));
    QCOMPARE(preview.get().filePaths, QStringList{outputPath});
    QCOMPARE(preview.get().warningFlags, quint32{0});
    QVERIFY(!QFile::exists(outputPath));

    const auto accepted = runtime().audioExports().start(commandContext(), reloadedConfig, {});
    QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
    const auto terminal = [&] {
        const auto task = runtime().tasks().getTask(before.documentId, accepted.get().taskId);
        return task && (task.get().state == Automation::AutomationTaskState::Succeeded ||
                        task.get().state == Automation::AutomationTaskState::Failed ||
                        task.get().state == Automation::AutomationTaskState::Canceled);
    };
    auto cleanupExport = qScopeGuard([&] {
        if (!terminal())
            runtime().tasks().cancelTask(commandContext(), accepted.get().taskId);
        runtime().audioExports().cleanup(commandContext(), accepted.get().taskId);
    });
    QTRY_VERIFY_WITH_TIMEOUT(terminal(), 10000);
    const auto task = runtime().tasks().getTask(before.documentId, accepted.get().taskId);
    QVERIFY(task);
    QVERIFY2(task.get().state == Automation::AutomationTaskState::Succeeded,
             qPrintable(task.get().error ? task.get().error->message : QString{}));
    {
        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        talcs::AudioFormatIO decoder(&output);
        QVERIFY2(decoder.open(talcs::AbstractAudioFormatIO::Read),
                 qPrintable(decoder.errorString()));
        QCOMPARE(decoder.majorFormat(), talcs::AudioFormatIO::WAV);
        QCOMPARE(decoder.subtype(), talcs::AudioFormatIO::PCM_16);
        QCOMPARE(decoder.sampleRate(), 44100.0);
        QCOMPARE(decoder.channelCount(), 1);
        QCOMPARE(decoder.length(), qint64{4410});
        QVector<float> samples(decoder.length());
        QCOMPARE(decoder.read(samples.data(), samples.size()), qint64(samples.size()));
        QVERIFY(std::all_of(samples.cbegin(), samples.cend(),
                            [](float sample) { return std::isfinite(sample); }));
        QVERIFY(samples.at(samples.size() / 2) > 0.0f);
        QVERIFY(std::abs(samples.at(samples.size() / 2) - samples.at(samples.size() * 3 / 4)) <=
                1.0f / 32768.0f);
    }
    QVERIFY(runtime().audioExports().cleanup(commandContext(), accepted.get().taskId));
    cleanupExport.dismiss();
    QVERIFY(!QFile::exists(files.filePath(QStringLiteral("draft.wav"))));

    QVERIFY(AudioExporter::removePreset(name));
    QVERIFY(!AudioExporter::presets().contains(name));
    const auto afterRemoval = runtime().settings().getSettings();
    QVERIFY(afterRemoval);
    QVERIFY(!AudioExporter::removePreset(name));
    QCOMPARE(Automation::toAutomationDto(AudioExporter::preset(name)),
             Automation::toAutomationDto(AudioExporterConfig{}));
    const auto afterMissingRemoval = runtime().settings().getSettings();
    QVERIFY(afterMissingRemoval);
    QCOMPARE(afterMissingRemoval.get().audio, afterRemoval.get().audio);
    AppOptions reopened;
    const auto persisted = Automation::createAppOptionsAutomationServices(&reopened).snapshot();
    QVERIFY(std::none_of(persisted.audio.audioExporterPresets.cbegin(),
                         persisted.audio.audioExporterPresets.cend(),
                         [&](const auto &preset) { return preset.name == name; }));
    QCOMPARE(runtime().documentVersion(), before);
    QCOMPARE(context->m_appModel->serialize(), beforeModel);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
}
