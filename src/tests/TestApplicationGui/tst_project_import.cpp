#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/TrackController.h"
#include "Global/TracksEditorGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/ProjectConverters/DspxConfigPage.h"
#include "Modules/ProjectConverters/MidiConfigPage.h"
#include "Modules/ProjectConverters/MidiBatchImportDialog.h"
#include "Modules/ProjectFormats/ProjectImportConfigDialog.h"
#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Views/TrackEditor/GraphicsItem/AbstractClipView.h"
#include "UI/Views/TrackEditor/TrackEditorView.h"
#include "UI/Views/TrackEditor/TracksGraphicsView.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectConverters/DspxProjectConverter.h>
#include <lite/ProjectConverters/MidiConverter.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/Tasking/TaskManager.h>

#include <QApplication>
#include <QCheckBox>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QScopeGuard>
#include <QStyle>
#include <QStyleOptionButton>
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

    void createImportFile(const QString &path, const bool midi, double tempo = 87.0) {
        AppModel source;
        source.setTimeline(Timeline(
            {
                {0, tempo}
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
        QStyleOptionButton option;
        option.initFrom(tempo);
        const auto indicator =
            tempo->style()->subElementRect(QStyle::SE_CheckBoxIndicator, &option, tempo);
        QTest::mouseClick(tempo, Qt::LeftButton, Qt::NoModifier, indicator.center());
        QVERIFY(!tempo->isChecked());
        const auto input = page->collectInput();
        QCOMPARE(input.tracks.selectedTrackIndices, QList<int>{selected.row()});
        QVERIFY(!input.timeline.importTempo);
        QVERIFY(input.timeline.importTimeSignature);
    }

    void selectUtf8Encoding(ComboBox *encoding) {
        QVERIFY(encoding);
        int utf8Index = -1;
        for (int index = 0; index < encoding->count(); ++index) {
            if (encoding->itemData(index).toByteArray().compare(QByteArrayLiteral("UTF-8"),
                                                                Qt::CaseInsensitive) == 0) {
                utf8Index = index;
                break;
            }
        }
        QVERIFY(utf8Index >= 0);
        QTest::mouseClick(encoding, Qt::LeftButton);
        QTRY_VERIFY(encoding->view()->isVisible());
        QTest::keyClick(encoding->view(), Qt::Key_Home);
        for (int index = 0; index < utf8Index; ++index)
            QTest::keyClick(encoding->view(), Qt::Key_Down);
        QTest::keyClick(encoding->view(), Qt::Key_Return);
        QCOMPARE(encoding->currentData().toByteArray().toUpper(), QByteArrayLiteral("UTF-8"));
    }

    void selectMidiEncoding(MidiConfigPage *page) {
        auto *preview = page->findChild<QTextEdit *>();
        QVERIFY(preview);
        selectUtf8Encoding(page->findChild<ComboBox *>());
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(page->selectedCodec().toUpper(), QByteArrayLiteral("UTF-8"));
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

void ApplicationGuiTests::droppingAudioFilesCommitsOneBatchToTheSelectedTracks() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QStringList paths{directory.filePath(QStringLiteral("first.wav")),
                            directory.filePath(QStringLiteral("second.wav"))};
    for (const auto &path : paths) {
        const auto error = createWaveFixture(path);
        QVERIFY2(error.isEmpty(), qPrintable(error));
    }

    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    const auto releaseAudio = qScopeGuard([&] {
        const auto reset = runtime.documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false));
        QTest::qVerify(bool(reset), "document reset", "release imported audio", __FILE__, __LINE__);
        const auto finished = QTest::qWaitFor([] { return taskManager->tasks().isEmpty(); }, 10000);
        QTest::qVerify(finished, "audio tasks finished", "release temporary audio files", __FILE__,
                       __LINE__);
    });
    TrackEditorView editor;
    auto *canvas = editor.findChild<TracksGraphicsView *>();
    QVERIFY(canvas);
    auto *previousDialogParent = Dialog::globalParent();
    Dialog::setGlobalContext(&editor);
    const auto clearDialogParent = qScopeGuard([&] {
        trackController->setParentWidget(nullptr);
        Dialog::setGlobalContext(previousDialogParent);
    });
    Automation::TrackDraftDto draft;
    draft.name = QStringLiteral("Drop destination");
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, draft));
    auto *existingTrack = appModel->tracks().first();
    const auto existingId = existingTrack->id();
    editor.resize(1200, 500);
    canvas->setAnimationEnabled(false);
    editor.show();
    editor.activateWindow();
    QTRY_VERIFY(editor.isVisible() && canvas->viewport()->width() > 600);
    QVERIFY(canvas->setViewportScale(2.0, 1.0));
    canvas->setViewportStartTick(0);
    QCoreApplication::processEvents();
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    historyManager->reset();
    const auto before = runtime.documentVersion();
    constexpr int dropTick = 960;
    const auto position = canvas->mapFromScene(
        QPointF(canvas->sceneXForTick(dropTick), TracksEditorGlobal::trackHeight / 2));
    QVERIFY(canvas->viewport()->rect().contains(position));
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(paths[0]), QUrl::fromLocalFile(paths[1])});

    QDragEnterEvent preview(position, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas->viewport(), &preview);
    QVERIFY(preview.isAccepted());
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(existingTrack->clips().count(), 0);
    QDragLeaveEvent leave;
    QApplication::sendEvent(canvas->viewport(), &leave);
    QVERIFY(leave.isAccepted());
    QVERIFY(!historyManager->canUndo());

    QDragEnterEvent enter(position, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas->viewport(), &enter);
    QVERIFY(enter.isAccepted());
    QDropEvent drop(QPointF(position), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas->viewport(), &drop);
    QVERIFY(drop.isAccepted());
    QTRY_COMPARE(appModel->tracks().size(), 2);
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    QCOMPARE(appModel->tracks().first()->id(), existingId);
    QCOMPARE(appModel->tracks().last()->name(), QStringLiteral("second"));
    for (qsizetype index = 0; index < paths.size(); ++index) {
        const auto *track = appModel->tracks().at(index);
        QCOMPARE(track->clips().count(), 1);
        const auto *clip = dynamic_cast<const AudioClip *>(*track->clips().begin());
        QVERIFY(clip);
        QCOMPARE(QFileInfo(clip->path()).canonicalFilePath(),
                 QFileInfo(paths[index]).canonicalFilePath());
        QCOMPARE(clip->start(), dropTick);
        QVERIFY(clip->length() > 0);
        QVERIFY(clip->audioInfo().frames > 0);
        auto *item = editor.findClipItemById(clip->id());
        QVERIFY(item);
        QCOMPARE(item->start(), clip->start());
        QCOMPARE(item->length(), clip->length());
    }
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QVERIFY(historyManager->canUndo());
    historyManager->undo();
    QCOMPARE(appModel->tracks().size(), 1);
    QCOMPARE(appModel->tracks().first()->id(), existingId);
    QCOMPARE(appModel->tracks().first()->clips().count(), 0);
    QVERIFY(!historyManager->canUndo());
    historyManager->redo();
    QCOMPARE(appModel->tracks().size(), 2);
    for (const auto *track : appModel->tracks()) {
        QCOMPARE(track->clips().count(), 1);
        const auto *clip = *track->clips().begin();
        QVERIFY(editor.findClipItemById(clip->id()));
        QCOMPARE(clip->start(), dropTick);
    }
    QTRY_VERIFY(taskManager->tasks().isEmpty());
}

void ApplicationGuiTests::droppingMidiAndAudioFilesUsesOneBatchDecision_data() {
    QTest::addColumn<bool>("accept");
    QTest::addColumn<bool>("importTempo");
    QTest::newRow("cancel-entire-mixed-batch") << false << true;
    QTest::newRow("first-midi-tempo-with-current-meter") << true << true;
    QTest::newRow("midi-meter-with-current-tempo") << true << false;
}

void ApplicationGuiTests::droppingMidiAndAudioFilesUsesOneBatchDecision() {
    QFETCH(bool, accept);
    QFETCH(bool, importTempo);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto firstMidi = directory.filePath(QStringLiteral("first.mid"));
    const auto secondMidi = directory.filePath(QStringLiteral("second.mid"));
    const auto audioPath = directory.filePath(QStringLiteral("audio.wav"));
    createImportFile(firstMidi, true);
    createImportFile(secondMidi, true, 103.0);
    if (QTest::currentTestFailed())
        return;
    const auto waveError = createWaveFixture(audioPath);
    QVERIFY2(waveError.isEmpty(), qPrintable(waveError));
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    const auto releaseAudio = qScopeGuard([&] {
        QVERIFY(runtime.documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    });
    TrackEditorView editor;
    auto *canvas = editor.findChild<TracksGraphicsView *>();
    QVERIFY(canvas);
    auto *previousDialogParent = Dialog::globalParent();
    Dialog::setGlobalContext(&editor);
    const auto clearParent = qScopeGuard([&] {
        trackController->setParentWidget(nullptr);
        Dialog::setGlobalContext(previousDialogParent);
    });
    Automation::TrackDraftDto existing;
    existing.name = QStringLiteral("Drop destination");
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, existing));
    const auto existingId = appModel->tracks().first()->id();
    editor.resize(1200, 600);
    canvas->setAnimationEnabled(false);
    editor.show();
    editor.activateWindow();
    QTRY_VERIFY(editor.isActiveWindow() && canvas->viewport()->width() > 600);
    QVERIFY(canvas->setViewportScale(2.0, 1.0));
    canvas->setViewportStartTick(0);
    QCoreApplication::processEvents();
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto beforeModel = appModel->serialize();
    const auto originalTimeline = appModel->timeline();
    int decisions = 0;
    QTimer answer;
    answer.setInterval(10);
    connect(&answer, &QTimer::timeout, &editor, [&] {
        auto *dialog = qobject_cast<MidiBatchImportDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        const auto close = qScopeGuard([&] {
            if (dialog->isVisible())
                dialog->reject();
        });
        ++decisions;
        QCOMPARE(decisions, 1);
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(appModel->serialize(), beforeModel);
        auto *codec = dialog->findChild<ComboBox *>();
        selectUtf8Encoding(codec);
        if (QTest::currentTestFailed())
            return;
        auto *tempo = withText<QCheckBox>(dialog, MidiBatchImportDialog::tr("Import tempo"));
        auto *meter =
            withText<QCheckBox>(dialog, MidiBatchImportDialog::tr("Import time signature"));
        QVERIFY(tempo && meter);
        QVERIFY(tempo->isChecked() && meter->isChecked());
        auto *unchecked = importTempo ? meter : tempo;
        unchecked->setFocus();
        QTest::keyClick(unchecked, Qt::Key_Space);
        QCOMPARE(tempo->isChecked(), importTempo);
        QCOMPARE(meter->isChecked(), !importTempo);
        auto *button = withText<Button>(dialog, accept ? MidiBatchImportDialog::tr("OK")
                                                       : MidiBatchImportDialog::tr("Cancel"));
        QVERIFY(button);
        QTest::mouseClick(button, Qt::LeftButton);
    });
    constexpr int dropTick = 960;
    const auto position = canvas->mapFromScene(
        QPointF(canvas->sceneXForTick(dropTick), TracksEditorGlobal::trackHeight / 2));
    QVERIFY(canvas->viewport()->rect().contains(position));
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(firstMidi), QUrl::fromLocalFile(secondMidi),
                  QUrl::fromLocalFile(audioPath)});
    QDragEnterEvent enter(position, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas->viewport(), &enter);
    QVERIFY(enter.isAccepted());
    answer.start();
    QDropEvent drop(QPointF(position), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas->viewport(), &drop);
    QVERIFY(drop.isAccepted());
    QTRY_VERIFY_WITH_TIMEOUT(decisions > 0, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    answer.stop();
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(decisions, 1);
    if (!accept) {
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(appModel->serialize(), beforeModel);
        QVERIFY(!historyManager->canUndo());
        return;
    }
    QCOMPARE(appModel->tracks().size(), 5);
    QCOMPARE(appModel->tracks().first()->id(), existingId);
    QCOMPARE(appModel->tracks().first()->name(), existing.name);
    // MIDI tempo uses an integer number of microseconds per quarter note.
    QVERIFY(qAbs(appModel->timeline().tempos().first().value - (importTempo ? 87.0 : 120.0)) <
            0.001);
    QCOMPARE(appModel->timeline().timeSignatures().first().numerator, importTempo ? 4 : 6);
    QCOMPARE(appModel->timeline().timeSignatures().first().denominator, importTempo ? 4 : 8);
    for (int index = 0; index < 4; ++index) {
        const auto *track = appModel->tracks().at(index);
        QCOMPARE(track->clips().count(), 1);
        const auto *clip = qobject_cast<SingingClip *>(*track->clips().begin());
        QVERIFY(clip);
        QCOMPARE(clip->notes().count(), 1);
        const auto *note = *clip->notes().begin();
        QCOMPARE(note->keyIndex(), index % 2 == 0 ? 60 : 64);
        QCOMPARE(note->localStart(), 240);
        QCOMPARE(note->length(), 480);
        QCOMPARE(note->lyric(),
                 index % 2 == 0 ? QStringLiteral("discard") : QStringLiteral("你好"));
        QVERIFY(editor.findClipItemById(clip->id()));
    }
    const auto *audioTrack = appModel->tracks().last();
    QCOMPARE(audioTrack->clips().count(), 1);
    const auto *audio = qobject_cast<AudioClip *>(*audioTrack->clips().begin());
    QVERIFY(audio);
    QCOMPARE(QFileInfo(audio->path()).canonicalFilePath(),
             QFileInfo(audioPath).canonicalFilePath());
    QCOMPARE(audio->start(), dropTick);
    QCOMPARE(audio->audioInfo().frames, 800);
    QVERIFY(qAbs(audio->length() - (importTempo ? 70 : 96)) <= 1);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(appModel->serialize(), beforeModel);
    QCOMPARE(appModel->timeline(), originalTimeline);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(runtime.history().redo(commandContext()));
    QCOMPARE(appModel->tracks().size(), 5);
    QCOMPARE(appModel->tracks().first()->id(), existingId);
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
}
