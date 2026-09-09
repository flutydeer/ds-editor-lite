#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/ProjectConverters/DspxConfigPage.h"
#include "Modules/ProjectConverters/MidiConfigPage.h"
#include "Modules/ProjectFormats/ProjectImportConfigDialog.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectConverters/DspxProjectConverter.h>
#include <lite/ProjectConverters/MidiConverter.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QApplication>
#include <QCheckBox>
#include <QScopeGuard>
#include <QTextEdit>
#include <QTimer>
#include <QTreeView>
#include <QtTest/QTest>

namespace {
    template <typename Widget>
    Widget *withText(QWidget *parent, const QString &text) {
        for (auto *widget : parent->findChildren<Widget *>()) {
            if (widget->text() == text)
                return widget;
        }
        return nullptr;
    }

    void createImportFile(const QString &path, const bool midi) {
        AppModel source;
        source.setTimeline(Timeline(
            {
                {0, 87.0}
        },
            {{0, 6, 8}}));
        for (int index = 0; index < 2; ++index) {
            auto *track = new Track;
            track->setName(index == 0 ? QStringLiteral("Not selected")
                                      : QStringLiteral("Imported lead"));
            auto *clip = new SingingClip;
            clip->setName(track->name());
            clip->setStart(0);
            clip->setClipStart(0);
            clip->setLength(1920);
            clip->setClipLen(1920);
            clip->setDefaultLanguage(QStringLiteral("cmn"));
            auto *note = new Note(clip);
            note->setLocalStart(240);
            note->setLength(480);
            note->setKeyIndex(index == 0 ? 60 : 64);
            note->setLyric(index == 0 ? QStringLiteral("discard") : QStringLiteral("你好"));
            clip->insertNote(note);
            track->insertClip(clip);
            QVERIFY(source.appendTrack(track));
        }
        QString error;
        if (midi) {
            MidiConverter converter;
            QVERIFY2(converter.save(path, &source, error), qPrintable(error));
        } else {
            DspxProjectConverter converter;
            QVERIFY2(converter.save(path, &source, error), qPrintable(error));
        }
    }

    template <typename Page>
    void selectImportContent(Page *page) {
        QVERIFY(page);
        auto *tracks = page->template findChild<QTreeView *>();
        auto *all = withText<QCheckBox>(page, Page::tr("Select All"));
        auto *tempo = withText<QCheckBox>(page, Page::tr("Import tempo"));
        auto *signature = withText<QCheckBox>(page, Page::tr("Import time signature"));
        QVERIFY(tracks);
        QVERIFY(all);
        QVERIFY(tempo);
        QVERIFY(signature);
        QCOMPARE(all->checkState(), Qt::Checked);
        QTest::mouseClick(all, Qt::LeftButton);
        QCOMPARE(all->checkState(), Qt::Unchecked);
        QVERIFY(page->selectedTracks().isEmpty());

        QModelIndex selected;
        for (int row = 0; row < tracks->model()->rowCount(); ++row) {
            const auto index = tracks->model()->index(row, 0);
            if (index.data().toString() == QStringLiteral("Imported lead"))
                selected = index;
        }
        QVERIFY(selected.isValid());
        tracks->scrollTo(selected);
        QTest::mouseClick(tracks->viewport(), Qt::LeftButton, Qt::NoModifier,
                          tracks->visualRect(selected).center());
        QTest::keyClick(tracks, Qt::Key_Space);
        QCOMPARE(page->selectedTracks(), QList<int>{selected.row()});
        QCOMPARE(all->checkState(), Qt::PartiallyChecked);
        QVERIFY(tempo->isChecked());
        QVERIFY(signature->isChecked());
        QTest::mouseClick(tempo, Qt::LeftButton);
        const auto input = page->collectInput();
        QCOMPARE(input.tracks.selectedTrackIndices, QList<int>{selected.row()});
        QVERIFY(!input.timeline.importTempo);
        QVERIFY(input.timeline.importTimeSignature);
    }

    void selectMidiEncoding(MidiConfigPage *page) {
        auto *encoding = page->findChild<ComboBox *>();
        auto *preview = page->findChild<QTextEdit *>();
        QVERIFY(encoding);
        QVERIFY(preview);
        const auto utf8Index = encoding->findData(QByteArrayLiteral("UTF-8"));
        QVERIFY(utf8Index >= 0);
        QTest::mouseClick(encoding, Qt::LeftButton);
        QTRY_VERIFY(encoding->view()->isVisible());
        QTest::keyClick(encoding->view(), Qt::Key_Home);
        for (int index = 0; index < utf8Index; ++index)
            QTest::keyClick(encoding->view(), Qt::Key_Down);
        QTest::keyClick(encoding->view(), Qt::Key_Return);
        QCOMPARE(page->selectedCodec(), QByteArrayLiteral("UTF-8"));
        QVERIFY(preview->isReadOnly());
        QCOMPARE(preview->toPlainText().trimmed(), QStringLiteral("你好"));
    }
}

void ApplicationGuiTests::interactiveProjectImportRespectsSelectionAndCancellation_data() {
    QTest::addColumn<bool>("midi");
    QTest::newRow("midi") << true;
    QTest::newRow("dspx") << false;
}

void ApplicationGuiTests::interactiveProjectImportRespectsSelectionAndCancellation() {
    QFETCH(bool, midi);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(midi ? QStringLiteral("selection.mid")
                                              : QStringLiteral("selection.dspx"));
    createImportFile(path, midi);
    if (QTest::currentTestFailed())
        return;

    auto &runtime = *context->m_coreRuntime;
    const auto before = runtime.documentVersion();
    const auto originalTimeline = appModel->timeline();
    const auto originalLoop = appStatus->loopSettings.get();
    const auto originalTracks = appModel->tracks();
    const auto originalProjectPath = documentWorkflowController->projectPath();
    historyManager->reset();

    const auto importWithConfirmation = [&](const bool accept) {
        bool configured = false;
        QTimer configureDialog;
        configureDialog.setInterval(10);
        connect(&configureDialog, &QTimer::timeout, this, [&] {
            auto *dialog =
                qobject_cast<ProjectImportConfigDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            configureDialog.stop();
            const auto cancelOnFailure = qScopeGuard([&] {
                if (!configured)
                    dialog->reject();
            });
            if (midi) {
                auto *page = qobject_cast<MidiConfigPage *>(dialog->page());
                selectImportContent(page);
                if (QTest::currentTestFailed())
                    return;
                selectMidiEncoding(page);
            } else {
                selectImportContent(qobject_cast<DspxConfigPage *>(dialog->page()));
            }
            if (QTest::currentTestFailed())
                return;
            auto *button =
                withText<Button>(dialog, ProjectImportConfigDialog::tr(accept ? "OK" : "Cancel"));
            QVERIFY(button);
            QTest::mouseClick(button, Qt::LeftButton);
            configured = true;
        });
        const auto cancelPending = qScopeGuard([&] {
            configureDialog.stop();
            if (documentWorkflowController->busy())
                documentWorkflowController->cancelCurrentOperation();
        });
        configureDialog.start();
        documentWorkflowController->requestImport(path);
        QTRY_VERIFY(!documentWorkflowController->busy());
        QVERIFY(configured);
    };

    importWithConfirmation(false);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(appModel->tracks(), originalTracks);
    QCOMPARE(appModel->timeline(), originalTimeline);
    QCOMPARE(appStatus->loopSettings.get(), originalLoop);
    QVERIFY(!historyManager->canUndo());

    importWithConfirmation(true);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(appModel->tracks().size(), originalTracks.size() + 1);
    for (qsizetype index = 0; index < originalTracks.size(); ++index)
        QCOMPARE(appModel->tracks().at(index), originalTracks.at(index));
    const auto *track = appModel->tracks().last();
    QCOMPARE(track->name(), QStringLiteral("Imported lead"));
    QCOMPARE(track->clips().count(), 1);
    const auto *clip = dynamic_cast<const SingingClip *>(*track->clips().begin());
    QVERIFY(clip);
    QCOMPARE(clip->notes().count(), 1);
    const auto *note = *clip->notes().begin();
    QCOMPARE(note->lyric(), QStringLiteral("你好"));
    QCOMPARE(note->globalStart(), 240);
    QCOMPARE(note->length(), 480);
    QCOMPARE(note->keyIndex(), 64);
    QCOMPARE(appModel->timeline().tempos(), originalTimeline.tempos());
    QCOMPARE(appModel->timeline().timeSignatures(), QList<TimeSignature>({
                                                        {0, 6, 8}
    }));
    QCOMPARE(appStatus->loopSettings.get(), originalLoop);
    QCOMPARE(documentWorkflowController->projectPath(), originalProjectPath);
    QCOMPARE(runtime.documentVersion().documentId, before.documentId);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QVERIFY(historyManager->canUndo());

    historyManager->undo();
    QCOMPARE(appModel->tracks(), originalTracks);
    QCOMPARE(appModel->timeline(), originalTimeline);
    QVERIFY(!historyManager->canUndo());
    historyManager->redo();
    QCOMPARE(appModel->tracks().size(), originalTracks.size() + 1);
    QCOMPARE(appModel->tracks().last()->name(), QStringLiteral("Imported lead"));
}
