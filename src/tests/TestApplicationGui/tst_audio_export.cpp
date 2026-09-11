#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Automation/OperationIds.h"
#include "UI/Dialogs/Audio/AudioExportDialog.h"
#include "UI/Dialogs/Audio/AudioExportProgressDialog.h"
#include "UI/Dialogs/Base/MessageDialog.h"

#include <lite/GUI/Controls/ProgressIndicator.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/Tasking/TaskManager.h>

#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTimer>
#include <QGroupBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QtTest/QTest>

#include <sndfile.h>

#include <array>

namespace {
    using Audio::AudioExporter;
    using Audio::AudioExporterConfig;

    struct ExportControls {
        explicit ExportControls(AudioExportDialog &dialog)
            : fileName(dialog.findChild<QLineEdit *>("audioExportFileName")),
              directory(dialog.findChild<QLineEdit *>("audioExportDirectory")),
              preview(dialog.findChild<QLabel *>("audioExportFileNamePreview")),
              fileType(dialog.findChild<QComboBox *>("audioExportFileType")),
              sampleRate(dialog.findChild<QComboBox *>("audioExportSampleRate")),
              mixing(dialog.findChild<QComboBox *>("audioExportMixing")),
              source(dialog.findChild<QComboBox *>("audioExportSource")),
              tracks(dialog.findChild<QListWidget *>("audioExportTracks")),
              cancel(dialog.findChild<QPushButton *>("audioExportCancel")),
              exporter(dialog.findChild<AudioExporter *>()) {
        }

        bool valid() const {
            return fileName && directory && preview && fileType && sampleRate && mixing && source &&
                   tracks && cancel && exporter;
        }

        QLineEdit *fileName;
        QLineEdit *directory;
        QLabel *preview;
        QComboBox *fileType;
        QComboBox *sampleRate;
        QComboBox *mixing;
        QComboBox *source;
        QListWidget *tracks;
        QPushButton *cancel;
        AudioExporter *exporter;
    };

    void pasteText(QLineEdit *edit, const QString &text) {
        edit->setFocus();
        QApplication::clipboard()->setText(text);
        QTest::keySequence(edit, QKeySequence::SelectAll);
        QTest::keySequence(edit, QKeySequence::Paste);
    }

    bool chooseOption(QComboBox *combo, int index) {
        combo->setFocus();
        QTest::keyClick(combo, Qt::Key_Home);
        for (int i = 0; i < index; ++i)
            QTest::keyClick(combo, Qt::Key_Down);
        return combo->currentIndex() == index;
    }

    QPushButton *exportButton(QWidget *parent, const QString &text) {
        for (auto *button : parent->findChildren<QPushButton *>()) {
            if (button->text() == text)
                return button;
        }
        return nullptr;
    }
}

QString ApplicationGuiTests::createWaveFixture(const QString &path) const {
    SF_INFO info{};
    info.samplerate = 8000;
    info.channels = 1;
    info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
#ifdef Q_OS_WIN
    auto *file = sf_wchar_open(reinterpret_cast<const wchar_t *>(path.utf16()), SFM_WRITE, &info);
#else
    auto *file = sf_open(QFile::encodeName(path).constData(), SFM_WRITE, &info);
#endif
    if (!file)
        return QString::fromUtf8(sf_strerror(nullptr));
    std::array<float, 800> samples{};
    for (size_t i = 0; i < samples.size(); ++i)
        samples[i] = i % 16 < 8 ? 0.25f : -0.25f;
    const auto written = sf_writef_float(file, samples.data(), samples.size());
    const auto closed = sf_close(file);
    if (written != sf_count_t(samples.size()) || closed != 0)
        return QStringLiteral("Cannot write the complete audio fixture: %1").arg(path);
    return {};
}

void ApplicationGuiTests::createExportTracks() {
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    for (const auto &name : {QStringLiteral("Lead"), QStringLiteral("Harmony")}) {
        Automation::TrackDraftDto draft;
        draft.name = name;
        draft.defaultLanguage = QStringLiteral("eng");
        QVERIFY(runtime.project().insertTrack(commandContext(),
                                              context->m_appModel->tracks().size(), draft));
    }
    historyManager->reset();
}

void ApplicationGuiTests::exportFormatUpdatesFileNamePreview() {
    createExportTracks();
    if (QTest::currentTestFailed())
        return;
    QTemporaryDir output;
    QVERIFY(output.isValid());
    auto &runtime = *context->m_coreRuntime;
    const auto before = runtime.documentVersion();
    const auto settings = runtime.settings().getSettings();
    QVERIFY(settings);
    AudioExportDialog dialog;
    ExportControls controls(dialog);
    QVERIFY(controls.valid());
    QSignalSpy started(&dialog, &AudioExportDialog::exportStarted);
    dialog.show();
    dialog.activateWindow();
    QTRY_VERIFY(dialog.isVisible());

    QVERIFY(chooseOption(controls.fileType, AudioExporterConfig::FT_Wav));
    pasteText(controls.directory, output.path());
    pasteText(controls.fileName, QStringLiteral("mix_${sampleRate}.wav"));
    QVERIFY(controls.sampleRate->lineEdit());
    pasteText(controls.sampleRate->lineEdit(), QStringLiteral("48000"));
    QCOMPARE(controls.exporter->config().formatSampleRate(), 48000.0);
    QCOMPARE(QFileInfo(controls.preview->text()).fileName(), QStringLiteral("mix_48000.wav"));
    QCOMPARE(QDir::cleanPath(QFileInfo(controls.preview->text()).absolutePath()),
             QDir::cleanPath(output.path()));

    QVERIFY(chooseOption(controls.fileType, AudioExporterConfig::FT_Flac));
    QCOMPARE(controls.fileName->text(), QStringLiteral("mix_${sampleRate}.flac"));
    QCOMPARE(controls.exporter->config().fileType(), AudioExporterConfig::FT_Flac);
    QCOMPARE(QFileInfo(controls.preview->text()).fileName(), QStringLiteral("mix_48000.flac"));
    QVERIFY(chooseOption(controls.fileType, AudioExporterConfig::FT_Wav));
    QCOMPARE(controls.fileName->text(), QStringLiteral("mix_${sampleRate}.wav"));
    QCOMPARE(QFileInfo(controls.preview->text()).fileName(), QStringLiteral("mix_48000.wav"));

    QCOMPARE(started.count(), 0);
    QCOMPARE(runtime.documentVersion(), before);
    const auto afterSettings = runtime.settings().getSettings();
    QVERIFY(afterSettings && afterSettings.get().audio == settings.get().audio);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(QDir(output.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());
}

void ApplicationGuiTests::exportSourcesAndMixingUpdateFilePlan() {
    createExportTracks();
    if (QTest::currentTestFailed())
        return;
    QTemporaryDir output;
    QVERIFY(output.isValid());
    auto &runtime = *context->m_coreRuntime;
    const auto before = runtime.documentVersion();
    AudioExportDialog dialog;
    ExportControls controls(dialog);
    QVERIFY(controls.valid());
    QSignalSpy started(&dialog, &AudioExportDialog::exportStarted);
    dialog.show();
    dialog.activateWindow();
    QTRY_VERIFY(dialog.isVisible());

    pasteText(controls.directory, output.path());
    pasteText(controls.fileName, QStringLiteral("stems.wav"));
    QVERIFY(chooseOption(controls.source, AudioExporterConfig::SO_All));
    QVERIFY(!controls.tracks->isEnabled());
    QCOMPARE(controls.tracks->count(), 2);
    QCOMPARE(controls.tracks->item(0)->text(), QStringLiteral("Lead"));
    QCOMPARE(controls.tracks->item(1)->text(), QStringLiteral("Harmony"));
    QVERIFY(chooseOption(controls.mixing, AudioExporterConfig::MO_Separated));
    QCOMPARE(controls.fileName->text(), QStringLiteral("stems_${trackIndex}_${trackName}.wav"));
    QCOMPARE(controls.exporter->dryRun(), QStringList({output.filePath("stems_1_Lead.wav"),
                                                       output.filePath("stems_2_Harmony.wav")}));

    QVERIFY(chooseOption(controls.source, AudioExporterConfig::SO_Custom));
    QVERIFY(controls.tracks->isEnabled());
    auto *harmony = controls.tracks->item(1);
    const auto itemRect = controls.tracks->visualItemRect(harmony);
    QVERIFY(controls.tracks->viewport()->rect().contains(itemRect.center()));
    QTest::mouseClick(controls.tracks->viewport(), Qt::LeftButton, Qt::NoModifier,
                      itemRect.center());
    QCOMPARE(controls.tracks->item(0)->checkState(), Qt::Unchecked);
    QCOMPARE(harmony->checkState(), Qt::Checked);
    QCOMPARE(controls.exporter->config().source(), QList<int>{1});
    QCOMPARE(controls.exporter->dryRun(), QStringList{output.filePath("stems_2_Harmony.wav")});
    QCOMPARE(QDir::cleanPath(controls.preview->text()),
             QDir::cleanPath(output.filePath("stems_2_Harmony.wav")));

    QVERIFY(chooseOption(controls.mixing, AudioExporterConfig::MO_Mixed));
    QCOMPARE(controls.fileName->text(), QStringLiteral("stems.wav"));
    QCOMPARE(controls.exporter->config().source(), QList<int>{1});
    QCOMPARE(controls.exporter->dryRun(), QStringList{output.filePath("stems.wav")});
    QCOMPARE(started.count(), 0);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(QDir(output.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());
}

void ApplicationGuiTests::exportPresetDialogsSaveOverwriteAndDeleteTheSelectedPreset() {
    createExportTracks();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    const auto settings = runtime.settings().getSettings();
    QVERIFY(settings);
    const auto nativeDialogsDisabled = QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    const auto restore = qScopeGuard([&] {
        QVERIFY(runtime.settings().updateAudio({}, settings.get().audio));
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, nativeDialogsDisabled);
    });
    const auto before = runtime.documentVersion();
    AudioExportDialog dialog;
    ExportControls controls(dialog);
    QVERIFY(controls.valid());
    auto *save = exportButton(&dialog, AudioExportDialog::tr("Save &As..."));
    auto *remove = exportButton(&dialog, AudioExportDialog::tr("&Delete"));
    QComboBox *presets = nullptr;
    for (auto *label : dialog.findChildren<QLabel *>()) {
        if (label->text() == AudioExportDialog::tr("&Preset"))
            presets = qobject_cast<QComboBox *>(label->buddy());
    }
    QVERIFY(save && remove && presets);
    QSignalSpy started(&dialog, &AudioExportDialog::exportStarted);
    dialog.show();
    dialog.activateWindow();
    QTRY_VERIFY(dialog.isActiveWindow());
    const QString name = QStringLiteral("Export preset from the dialog");
    QVERIFY(!AudioExporter::presets().contains(name));
    QVERIFY(chooseOption(controls.fileType, AudioExporterConfig::FT_Wav));
    pasteText(controls.fileName, QStringLiteral("initial.wav"));
    const auto saveAs = [&](bool acceptName, QMessageBox::StandardButton overwrite) {
        int namePrompts = 0;
        QTimer answer;
        answer.setInterval(10);
        connect(&answer, &QTimer::timeout, &dialog, [&] {
            auto *active = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!active || active == &dialog)
                return;
            const auto close = qScopeGuard([&] {
                if (active->isVisible())
                    active->reject();
            });
            if (auto *input = qobject_cast<QInputDialog *>(active)) {
                ++namePrompts;
                auto *text = input->findChild<QLineEdit *>();
                QVERIFY(text);
                if (!acceptName || namePrompts > 1) {
                    QTest::keyClick(text, Qt::Key_Escape);
                } else {
                    pasteText(text, name);
                    QTest::keyClick(text, Qt::Key_Return);
                }
            } else if (auto *question = qobject_cast<QMessageBox *>(active)) {
                auto *choice = question->button(overwrite);
                QVERIFY(choice);
                QTest::mouseClick(choice, Qt::LeftButton);
            }
        });
        answer.start();
        QTest::mouseClick(save, Qt::LeftButton);
        QVERIFY(namePrompts > 0);
    };
    saveAs(false, QMessageBox::Yes);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(!AudioExporter::presets().contains(name));
    saveAs(true, QMessageBox::Yes);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(presets->currentData().toString(), name);
    QCOMPARE(AudioExporter::preset(name).fileName(), QStringLiteral("initial.wav"));
    QVERIFY(remove->isEnabled());
    pasteText(controls.fileName, QStringLiteral("updated.wav"));
    saveAs(true, QMessageBox::No);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(AudioExporter::preset(name).fileName(), QStringLiteral("initial.wav"));
    QCOMPARE(controls.fileName->text(), QStringLiteral("updated.wav"));
    saveAs(true, QMessageBox::Yes);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(AudioExporter::preset(name).fileName(), QStringLiteral("updated.wav"));
    QCOMPARE(presets->currentData().toString(), name);
    QTest::mouseClick(remove, Qt::LeftButton);
    QVERIFY(!AudioExporter::presets().contains(name));
    QCOMPARE(presets->findData(name), -1);
    QVERIFY(!remove->isEnabled());
    QVERIFY(started.isEmpty());
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::canceledExportConfigurationDoesNotPersist() {
    createExportTracks();
    if (QTest::currentTestFailed())
        return;
    QTemporaryDir output;
    QVERIFY(output.isValid());
    auto &runtime = *context->m_coreRuntime;
    const auto before = runtime.documentVersion();
    const auto settings = runtime.settings().getSettings();
    QVERIFY(settings);
    QVariantMap originalConfiguration;
    {
        AudioExportDialog dialog;
        ExportControls controls(dialog);
        QVERIFY(controls.valid());
        originalConfiguration = controls.exporter->config().toVariantMap();
        QSignalSpy started(&dialog, &AudioExportDialog::exportStarted);
        QSignalSpy rejected(&dialog, &QDialog::rejected);
        dialog.show();
        dialog.activateWindow();
        QTRY_VERIFY(dialog.isVisible());

        pasteText(controls.directory, output.path());
        pasteText(controls.fileName, QStringLiteral("discard-me.wav"));
        QVERIFY(chooseOption(controls.fileType, AudioExporterConfig::FT_Flac));
        QVERIFY(chooseOption(controls.mixing, AudioExporterConfig::MO_Separated));
        QVERIFY(controls.exporter->config().toVariantMap() != originalConfiguration);
        QTest::mouseClick(controls.cancel, Qt::LeftButton);
        QTRY_VERIFY(!dialog.isVisible());
        QCOMPARE(rejected.count(), 1);
        QCOMPARE(started.count(), 0);
    }

    AudioExportDialog reopened;
    ExportControls controls(reopened);
    QVERIFY(controls.valid());
    reopened.show();
    reopened.activateWindow();
    QTRY_VERIFY(reopened.isVisible());
    QCOMPARE(controls.exporter->config().toVariantMap(), originalConfiguration);
    QCOMPARE(controls.fileName->text(), originalConfiguration.value("fileName").toString());
    QCOMPARE(controls.directory->text(), originalConfiguration.value("fileDirectory").toString());
    QCOMPARE(controls.fileType->currentIndex(), originalConfiguration.value("fileType").toInt());
    QCOMPARE(controls.mixing->currentIndex(), originalConfiguration.value("mixingOption").toInt());
    QCOMPARE(runtime.documentVersion(), before);
    const auto afterSettings = runtime.settings().getSettings();
    QVERIFY(afterSettings && afterSettings.get().audio == settings.get().audio);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(QDir(output.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());
}

void ApplicationGuiTests::audioExportProgressCompletesAndCloses() {
    using Audio::Internal::AudioExportProgressDialog;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto inputPath = directory.filePath(QStringLiteral("input.wav"));
    const auto error = createWaveFixture(inputPath);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const auto outputPath = directory.filePath(QStringLiteral("exported.wav"));
    const QByteArray existingContents = "Preserve this file until export is confirmed";
    QFile existing(outputPath);
    QVERIFY(existing.open(QIODevice::WriteOnly));
    QCOMPARE(existing.write(existingContents), existingContents.size());
    existing.close();
    const auto readExisting = [&] {
        QFile file(outputPath);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    };
    auto &runtime = *context->m_coreRuntime;
    const auto originalSettings = runtime.settings().getSettings();
    QVERIFY(originalSettings);
    const auto releaseAudio = qScopeGuard([&] {
        runtime.settings().updateAudio({}, originalSettings.get().audio);
        const auto reset = runtime.documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false));
        QTest::qVerify(bool(reset), "document reset", "release the temporary audio document",
                       __FILE__, __LINE__);
        const auto finished = QTest::qWaitFor([] { return taskManager->tasks().isEmpty(); }, 10000);
        QTest::qVerify(finished, "audio tasks finished", "release the audio export fixture",
                       __FILE__, __LINE__);
        if (QTest::currentTestFailed()) {
            directory.setAutoRemove(false);
            qWarning() << "Audio export fixture retained at" << directory.path();
        }
    });
    auto draft = Automation::DocumentAutomationFacade::newDocumentDraft(false);
    Automation::TrackDraftDto track;
    track.name = QStringLiteral("Audio export");
    Automation::ClipDraftDto clip;
    clip.type = Automation::ClipDraftDto::Type::Audio;
    clip.properties.name = QStringLiteral("Audio fixture");
    clip.properties.length = 480;
    clip.properties.clipLen = 480;
    clip.audioPath = inputPath;
    track.clips.append(clip);
    draft.tracks.append(track);
    QVERIFY(runtime.documents().commitNewDocument(commandContext(), draft));
    auto *audio = qobject_cast<AudioClip *>(*appModel->tracks().first()->clips().begin());
    QVERIFY(audio);
    QTRY_VERIFY(audio->audioInfo().frames > 0 && !audio->audioInfo().peakCache.isEmpty());
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    historyManager->reset();
    const auto before = runtime.documentVersion();

    AudioExportDialog dialog;
    ExportControls controls(dialog);
    QVERIFY(controls.valid());
    auto *start = exportButton(&dialog, AudioExportDialog::tr("Export"));
    QVERIFY(start);
    QCheckBox *keepOpen = nullptr;
    for (auto *checkbox : dialog.findChildren<QCheckBox *>()) {
        if (checkbox->text() ==
            AudioExportDialog::tr("&Keep this dialog open after successful export"))
            keepOpen = checkbox;
    }
    QVERIFY(keepOpen);
    QVERIFY(!keepOpen->isChecked());
    QSignalSpy succeeded(&dialog, &AudioExportDialog::exportFinished);
    QSignalSpy started(&dialog, &AudioExportDialog::exportStarted);
    QSignalSpy failed(&dialog, &AudioExportDialog::exportFailed);
    QSignalSpy dismissed(&dialog, &AudioExportDialog::exportDismissed);
    QSignalSpy accepted(&dialog, &QDialog::accepted);
    dialog.show();
    dialog.activateWindow();
    QTRY_VERIFY(dialog.isActiveWindow());
    QVERIFY(chooseOption(controls.fileType, AudioExporterConfig::FT_Wav));
    QVERIFY(chooseOption(controls.mixing, AudioExporterConfig::MO_Mixed));
    QVERIFY(chooseOption(controls.source, AudioExporterConfig::SO_All));
    pasteText(controls.directory, directory.path());
    pasteText(controls.fileName, QStringLiteral("exported.wav"));
    QVERIFY(controls.exporter->warning() & AudioExporter::W_WillOverwrite);
    bool previewInspected = false;
    QTimer inspectPreview;
    inspectPreview.setInterval(10);
    connect(&inspectPreview, &QTimer::timeout, &dialog, [&] {
        QPointer<QDialog> preview = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!preview || preview->windowTitle() != AudioExportDialog::tr("Dry Run"))
            return;
        inspectPreview.stop();
        const auto close = qScopeGuard([&] {
            if (preview && preview->isVisible())
                preview->reject();
        });
        QListWidget *files = nullptr;
        for (auto *group : preview->findChildren<QGroupBox *>()) {
            if (group->title() == AudioExportDialog::tr("File List"))
                files = group->findChild<QListWidget *>();
        }
        QVERIFY(files);
        QCOMPARE(files->count(), 1);
        QCOMPARE(QDir::fromNativeSeparators(files->item(0)->text()), outputPath);
        QVERIFY(!files->item(0)->icon().isNull());
        QVERIFY(files->item(0)->toolTip().contains(
            AudioExporter::warningText(AudioExporter::W_WillOverwrite).first()));
        auto *ok = exportButton(preview, AudioExportDialog::tr("OK"));
        QVERIFY(ok);
        QTest::mouseClick(ok, Qt::LeftButton);
        previewInspected = true;
    });
    auto *dryRun = exportButton(&dialog, AudioExportDialog::tr("Dry &Run"));
    QVERIFY(dryRun);
    inspectPreview.start();
    QTest::mouseClick(dryRun, Qt::LeftButton);
    QVERIFY(previewInspected);
    QCOMPARE(readExisting(), existingContents);
    QCOMPARE(runtime.documentVersion(), before);

    bool allowOverwrite = false;
    int confirmations = 0;
    QTimer answerWarning;
    answerWarning.setInterval(10);
    connect(&answerWarning, &QTimer::timeout, &dialog, [&] {
        QPointer<MessageDialog> warning =
            qobject_cast<MessageDialog *>(QApplication::activeModalWidget());
        if (!warning)
            return;
        answerWarning.stop();
        const auto close = qScopeGuard([&] {
            if (warning && warning->isVisible())
                warning->reject();
        });
        auto *choice =
            exportButton(warning, AudioExportDialog::tr(allowOverwrite ? "Continue" : "Cancel"));
        QVERIFY(choice);
        QCOMPARE(readExisting(), existingContents);
        QTest::mouseClick(choice, Qt::LeftButton);
        ++confirmations;
    });
    answerWarning.start();
    QTest::mouseClick(start, Qt::LeftButton);
    QCOMPARE(confirmations, 1);
    QVERIFY(started.isEmpty());
    QVERIFY(dialog.isVisible());
    QCOMPARE(readExisting(), existingContents);
    QCOMPARE(runtime.documentVersion(), before);

    QPointer<AudioExportProgressDialog> progress;
    const auto releaseProgress = qScopeGuard([&] {
        if (progress)
            delete progress.data();
    });
    QTimer deadline;
    deadline.setSingleShot(true);
    bool timedOut = false;
    connect(&deadline, &QTimer::timeout, &dialog, [&] {
        timedOut = true;
        controls.exporter->cancel();
    });
    allowOverwrite = true;
    answerWarning.start();
    QTest::mouseClick(start, Qt::LeftButton);
    QCOMPARE(confirmations, 2);
    QCOMPARE(started.size(), 1);
    for (auto *window : QApplication::topLevelWidgets()) {
        if (auto *candidate = qobject_cast<AudioExportProgressDialog *>(window))
            progress = candidate;
    }
    QVERIFY(progress);
    QVERIFY(progress->isVisible());
    deadline.start(15000);
    QTRY_VERIFY_WITH_TIMEOUT(!progress || progress->isTerminal() || timedOut, 15000);
    deadline.stop();
    QVERIFY2(!timedOut, "The audio export did not reach a terminal state");
    QVERIFY(progress);
    QVERIFY2(failed.isEmpty(), qPrintable(controls.exporter->errorString()));
    QCOMPARE(succeeded.size(), 1);
    QVERIFY(!dialog.isVisible());
    QVERIFY(progress->isVisible());
    QVERIFY(!progress->isModal());
    QCOMPARE(progress->windowTitle(), AudioExportProgressDialog::tr("Export finished"));
    auto *indicator = progress->findChild<ProgressIndicator *>();
    QVERIFY(indicator);
    QCOMPARE(indicator->value(), 100.0);
    auto *close = exportButton(progress, AudioExportProgressDialog::tr("Close"));
    auto *cancel = exportButton(progress, AudioExportProgressDialog::tr("Cancel"));
    QVERIFY(close && close->isVisible() && close->isEnabled());
    QVERIFY(cancel && !cancel->isVisible() && !cancel->isEnabled());
    QVERIFY(QFileInfo(outputPath).isFile());
    QVERIFY(QFileInfo(outputPath).size() > 44);
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    const auto tasks = runtime.tasks().listTasks(before.documentId);
    QVERIFY(tasks);
    bool exportWasCleaned = false;
    for (const auto &task : tasks.get()) {
        if (task.operationId != Automation::OperationIds::exports::audio::start)
            continue;
        QCOMPARE(task.state, Automation::AutomationTaskState::Succeeded);
        auto query = commandContext();
        query.validateOnly = true;
        const auto cleanup = runtime.audioExports().cleanup(query, task.taskId);
        QVERIFY(cleanup);
        QVERIFY(!cleanup.get().changed);
        exportWasCleaned = true;
    }
    QVERIFY(exportWasCleaned);
    QTest::mouseClick(close, Qt::LeftButton);
    QCOMPARE(dismissed.size(), 1);
    QCOMPARE(accepted.size(), 1);
    QTRY_VERIFY(progress.isNull());
    QVERIFY(!dialog.isVisible());
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}
