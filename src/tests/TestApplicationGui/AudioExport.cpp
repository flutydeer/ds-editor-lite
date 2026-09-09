#include "ApplicationGuiTests.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "UI/Dialogs/Audio/AudioExportDialog.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QTest>

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
}

void ApplicationGuiTests::createExportTracks() {
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    for (const auto &name : {QStringLiteral("Lead"), QStringLiteral("Harmony")}) {
        Automation::TrackDraftDto draft;
        draft.name = name;
        draft.defaultLanguage = QStringLiteral("eng");
        QVERIFY(runtime.project().insertTrack(commandContext(), context->m_appModel->tracks().size(),
                                              draft));
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
    QCOMPARE(controls.exporter->dryRun(),
             QStringList({output.filePath("stems_1_Lead.wav"),
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
