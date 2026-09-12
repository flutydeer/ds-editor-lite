#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/ClipController.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/TrackController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "Modules/Inference/InferController.h"
#include "UI/Dialogs/Note/PhonemeEditorDialog.h"
#include "UI/Dialogs/Note/PhonemeNameItemView.h"
#include "UI/Dialogs/Note/PhonemeNameListWidget.h"
#include "UI/Dialogs/Base/MessageDialog.h"
#include "UI/Dialogs/Search/SearchDialog.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollCoord.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollView.h"
#include "UI/Views/ClipEditor/PianoRoll/PhonemeView.h"
#include "UI/Views/ClipEditor/ClipEditorView.h"
#include "UI/Window/MainWindow.h"

#include <lite/GUI/Controls/AccentButton.h>
#include <lite/GUI/Controls/LineEdit.h>
#include <lite/GUI/Controls/Toast.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/Tasking/TaskManager.h>

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSemaphore>
#include <QThreadPool>
#include <QTimer>
#include <QtTest/QTest>

#include <algorithm>

namespace {
    void enterNoteText(QLineEdit *editor, const QString &text) {
        QVERIFY(editor);
        QTest::mouseClick(editor, Qt::LeftButton);
        QTRY_VERIFY(editor->hasFocus());
        QTest::keySequence(editor, QKeySequence::SelectAll);
        if (text.isEmpty()) {
            QTest::keyClick(editor, Qt::Key_Backspace);
        } else {
            QApplication::clipboard()->setText(text);
            QTest::keySequence(editor, QKeySequence::Paste);
        }
        QCOMPARE(editor->text(), text);
    }

    QPushButton *noteDialogButton(QWidget *parent, const QString &text) {
        for (auto *button : parent->findChildren<QPushButton *>()) {
            if (button->text() == text)
                return button;
        }
        return nullptr;
    }
}

void ApplicationGuiTests::noteLanguageMenuChangesOnlyTheSelectedWords_data() {
    QTest::addColumn<int>("choice");
    QTest::newRow("explicit-language") << 0;
    QTest::newRow("follow-singer") << 1;
    QTest::newRow("unknown-language") << 2;
}

void ApplicationGuiTests::noteLanguageMenuChangesOnlyTheSelectedWords() {
    QFETCH(int, choice);
    createLyricSelection();
    if (QTest::currentTestFailed())
        return;
    const auto notes = singingClip->notes().toList();
    QCOMPARE(notes.size(), 3);
    const auto originalLanguage = notes.first()->language();
    QString selectedLanguage;
    if (choice == 0) {
        for (const auto &language : singingClip->singerInfo().languages()) {
            if (language.id() != originalLanguage) {
                selectedLanguage = language.id();
                break;
            }
        }
        if (selectedLanguage.isEmpty())
            QSKIP("The configured voicebank has no second language");
    } else if (choice == 2) {
        selectedLanguage = QStringLiteral("unknown");
    }
    auto &runtime = *context->m_coreRuntime;
    const auto clipId = Automation::ClipId(singingClip->id());
    const auto manualPronunciation = notes.first()->pronunciation().result();
    QVERIFY(!manualPronunciation.isEmpty());
    QVERIFY(runtime.notes().setPronunciation(commandContext(), clipId,
                                             Automation::NoteId(notes.first()->id()), false,
                                             manualPronunciation));
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    QCOMPARE(notes.first()->pronunciation().edited, manualPronunciation);
    const auto untouched = notes.last()->serialize();
    const auto firstLyric = notes.first()->lyric();
    const auto secondLyric = notes.at(1)->lyric();
    view->hide();
    PianoRollView piano;
    piano.setDataContext(singingClip);
    const auto detach = qScopeGuard([&] { piano.setDataContext(nullptr); });
    piano.resize(1000, 600);
    piano.show();
    piano.activateWindow();
    QVERIFY(piano.setViewScale(1.0, 1.0));
    QVERIFY(piano.centerAt(1920, 60));
    piano.onEditModeChanged(ClipEditorGlobal::Select);
    auto *canvas = piano.findChild<PianoRollGraphicsView *>();
    QVERIFY(canvas);
    QTRY_VERIFY(canvas->isVisible());
    const auto pointAt = [&](int tick) {
        return canvas->mapFromScene(QPointF(
            canvas->tickToSceneX(tick), PianoRollCoord::keyIndexToCenterY(
                                            60, ClipEditorGlobal::noteHeight * canvas->scaleY())));
    };
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, pointAt(240));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::ControlModifier, pointAt(720));
    QCOMPARE(appStatus->selectedNotes.get().size(), 2);
    QVERIFY(appStatus->selectedNotes.get().contains(notes.first()->id()));
    QVERIFY(appStatus->selectedNotes.get().contains(notes.at(1)->id()));
    historyManager->reset();
    const auto before = runtime.documentVersion();
    bool chosen = false;
    QTimer choose;
    choose.setSingleShot(true);
    connect(&choose, &QTimer::timeout, &piano, [&] {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        QVERIFY(menu);
        const auto close = qScopeGuard([&] { menu->close(); });
        QMenu *languages = nullptr;
        for (auto *action : menu->actions()) {
            if (action->text() == PianoRollContextMenuController::tr("Language"))
                languages = action->menu();
        }
        QVERIFY(languages);
        const auto closeLanguages = qScopeGuard([&] { languages->close(); });
        QString text = selectedLanguage;
        if (choice == 1)
            text = PianoRollContextMenuController::tr("Follow singer");
        else if (choice == 2)
            text = PianoRollContextMenuController::tr("Unknown");
        else if (selectedLanguage == singingClip->singerInfo().defaultLanguage())
            text += PianoRollContextMenuController::tr(" (default)");
        QAction *target = nullptr;
        for (auto *action : languages->actions()) {
            if (action->text() == text)
                target = action;
        }
        QVERIFY(target);
        QVERIFY(target->isEnabled());
        QVERIFY(!target->isChecked());
        QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                          menu->actionGeometry(languages->menuAction()).center());
        QTRY_VERIFY(languages->isVisible());
        QTest::mouseClick(languages, Qt::LeftButton, Qt::NoModifier,
                          languages->actionGeometry(target).center());
        chosen = true;
    });
    const auto point = pointAt(240);
    QContextMenuEvent event(QContextMenuEvent::Mouse, point,
                            canvas->viewport()->mapToGlobal(point));
    choose.start(0);
    QApplication::sendEvent(canvas->viewport(), &event);
    choose.stop();
    if (QTest::currentTestFailed())
        return;
    QVERIFY(chosen);
    QCOMPARE(notes.first()->language(), selectedLanguage);
    QCOMPARE(notes.at(1)->language(), selectedLanguage);
    QCOMPARE(notes.first()->lyric(), firstLyric);
    QCOMPARE(notes.at(1)->lyric(), secondLyric);
    QVERIFY(notes.first()->pronunciation().edited.isEmpty());
    QCOMPARE(notes.last()->serialize(), untouched);
    // Language tasks also update derived pronunciations; the user edit has one undo entry.
    QVERIFY(runtime.documentVersion().revision > before.revision);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(notes.first()->language(), originalLanguage);
    QCOMPARE(notes.at(1)->language(), originalLanguage);
    QCOMPARE(notes.first()->pronunciation().edited, manualPronunciation);
    QCOMPARE(notes.last()->serialize(), untouched);
    QVERIFY(!historyManager->canUndo());
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
}

void ApplicationGuiTests::phonemeDurationResetConfirmsAdjacentChanges_data() {
    QTest::addColumn<bool>("accept");
    QTest::addColumn<bool>("selectBoth");
    QTest::newRow("cancel-adjacent-reset") << false << false;
    QTest::newRow("confirm-adjacent-reset") << true << false;
    QTest::newRow("selected-neighbours-need-no-prompt") << true << true;
}

void ApplicationGuiTests::phonemeDurationResetConfirmsAdjacentChanges() {
    QFETCH(bool, accept);
    QFETCH(bool, selectBoth);
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    const auto clipId = Automation::ClipId(singingClip->id());
    Automation::NoteDraftDto first;
    first.localStart = 73;
    first.length = 407;
    first.keyIndex = 60;
    first.lyric = QStringLiteral("first");
    first.language = QStringLiteral("eng");
    auto second = first;
    second.localStart = 600;
    second.length = 360;
    second.keyIndex = 64;
    second.lyric = QStringLiteral("neighbour");
    QVERIFY(runtime.notes().insertNotes(commandContext(), clipId, {first, second}));
    QCOMPARE(singingClip->notes().count(), 2);
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    const auto notes = singingClip->notes().toList();
    PhonemeName onset;
    onset.language = QStringLiteral("eng");
    onset.name = QStringLiteral("l");
    onset.isOnset = true;
    PhonemeName vowel;
    vowel.language = QStringLiteral("eng");
    vowel.name = QStringLiteral("a");
    Phonemes firstPhones;
    firstPhones.nameSeq.original = {onset, vowel};
    firstPhones.nameSeq.edited = {onset, vowel};
    firstPhones.offsetSeq.original = {-40, 700};
    firstPhones.offsetSeq.edited = {-20, 350};
    auto secondPhones = firstPhones;
    secondPhones.offsetSeq.original = {0, 200};
    secondPhones.offsetSeq.edited = {-50, 200};
    QVERIFY(runtime.notes().setPhonemes(commandContext(), clipId,
                                        Automation::NoteId(notes.first()->id()), firstPhones));
    QVERIFY(runtime.notes().setPhonemes(commandContext(), clipId,
                                        Automation::NoteId(notes.last()->id()), secondPhones));
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    view->hide();
    PianoRollView piano;
    piano.setDataContext(singingClip);
    const auto detach = qScopeGuard([&] { piano.setDataContext(nullptr); });
    piano.resize(1000, 600);
    piano.show();
    piano.activateWindow();
    QVERIFY(piano.setViewScale(1.0, 1.0));
    QVERIFY(piano.centerAt(1920, 62));
    piano.onEditModeChanged(ClipEditorGlobal::Select);
    auto *canvas = piano.findChild<PianoRollGraphicsView *>();
    QVERIFY(canvas);
    QTRY_VERIFY(canvas->isVisible());
    const auto pointAt = [&](int tick, int key) {
        return canvas->mapFromScene(QPointF(
            canvas->tickToSceneX(tick), PianoRollCoord::keyIndexToCenterY(
                                            key, ClipEditorGlobal::noteHeight * canvas->scaleY())));
    };
    const auto position = pointAt(300, 60);
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, position);
    if (selectBoth)
        QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::ControlModifier,
                          pointAt(780, 64));
    QCOMPARE(appStatus->selectedNotes.get().size(), selectBoth ? 2 : 1);
    QVERIFY(appStatus->selectedNotes.get().contains(notes.first()->id()));
    historyManager->reset();
    const auto before = runtime.documentVersion();
    bool promptSeen = false;
    bool actionChosen = false;
    QTimer answer;
    answer.setInterval(10);
    connect(&answer, &QTimer::timeout, &piano, [&] {
        auto *prompt = qobject_cast<MessageDialog *>(QApplication::activeModalWidget());
        if (!prompt)
            return;
        answer.stop();
        promptSeen = true;
        const auto close = qScopeGuard([&] {
            if (prompt->isVisible())
                prompt->reject();
        });
        QVERIFY(!selectBoth);
        QCOMPARE(prompt->windowTitle(), ClipController::tr("Reset phoneme durations"));
        const auto labels = prompt->findChildren<QLabel *>();
        QVERIFY(std::any_of(labels.cbegin(), labels.cend(), [](const auto *label) {
            return label->text().contains(QStringLiteral("neighbour"));
        }));
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(notes.last()->phonemes().offsetSeq.edited, secondPhones.offsetSeq.edited);
        auto *button = noteDialogButton(prompt, accept ? ClipController::tr("Reset")
                                                       : ClipController::tr("Cancel"));
        QVERIFY(button);
        QTest::mouseClick(button, Qt::LeftButton);
    });
    QTimer choose;
    choose.setSingleShot(true);
    connect(&choose, &QTimer::timeout, &piano, [&] {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        QVERIFY(menu);
        const auto close = qScopeGuard([&] { menu->close(); });
        for (auto *action : menu->actions()) {
            if (action->text() == PianoRollContextMenuController::tr("Reset Phoneme Durations")) {
                QVERIFY(action->isEnabled());
                actionChosen = true;
                QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                                  menu->actionGeometry(action).center());
                return;
            }
        }
        QFAIL("The note menu did not expose phoneme timing reset");
    });
    QContextMenuEvent event(QContextMenuEvent::Mouse, position,
                            canvas->viewport()->mapToGlobal(position));
    answer.start();
    choose.start(0);
    QApplication::sendEvent(canvas->viewport(), &event);
    choose.stop();
    answer.stop();
    if (QTest::currentTestFailed())
        return;
    QVERIFY(actionChosen);
    QCOMPARE(promptSeen, !selectBoth);
    if (accept) {
        QVERIFY(notes.first()->phonemes().offsetSeq.edited.isEmpty());
        QVERIFY(notes.last()->phonemes().offsetSeq.edited.isEmpty());
        QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
        QVERIFY(runtime.history().undo(commandContext()));
    } else {
        QCOMPARE(runtime.documentVersion(), before);
    }
    QCOMPARE(notes.first()->phonemes().offsetSeq.edited, firstPhones.offsetSeq.edited);
    QCOMPARE(notes.last()->phonemes().offsetSeq.edited, secondPhones.offsetSeq.edited);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::phonemeDialogValidatesCommitsAndResetsThroughTheNoteMenu() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    Automation::NoteDraftDto draft;
    draft.localStart = 480;
    draft.length = 480;
    draft.keyIndex = 62;
    draft.lyric = QStringLiteral("la");
    draft.language = QStringLiteral("cmn");
    PhonemeName consonant;
    consonant.language = draft.language;
    consonant.name = QStringLiteral("l");
    PhonemeName vowel;
    vowel.language = draft.language;
    vowel.name = QStringLiteral("a");
    vowel.isOnset = true;
    draft.phonemes.nameSeq.original = {consonant, vowel};
    auto &runtime = *context->m_coreRuntime;
    const auto inserted = runtime.notes().insertNotes(
        commandContext(), Automation::ClipId(singingClip->id()), {draft});
    QVERIFY(inserted);
    QCOMPARE(singingClip->notes().count(), 1);
    auto *note = *singingClip->notes().begin();
    appStatus->selectedNotes = QList<int>{note->id()};
    view->hide();
    PianoRollView pianoRoll;
    pianoRoll.setDataContext(singingClip);
    const auto detachPianoRoll = qScopeGuard([&] { pianoRoll.setDataContext(nullptr); });
    pianoRoll.resize(900, 600);
    pianoRoll.show();
    pianoRoll.activateWindow();
    QVERIFY(pianoRoll.setViewScale(1.0, 1.0));
    QVERIFY(pianoRoll.centerAt(1920, 60));
    auto *canvas = pianoRoll.findChild<PianoRollGraphicsView *>();
    QVERIFY(canvas);
    QTRY_VERIFY(canvas->isVisible());
    historyManager->reset();
    const auto before = runtime.documentVersion();
    QPointer<PhonemeEditorDialog> dialog;

    const auto openEditor = [&] {
        bool selected = false;
        QTimer chooseAction;
        chooseAction.setSingleShot(true);
        connect(&chooseAction, &QTimer::timeout, &pianoRoll, [&] {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(menu);
            const auto closeOnFailure = qScopeGuard([&] {
                if (!selected)
                    menu->close();
            });
            QAction *edit = nullptr;
            for (auto *action : menu->actions()) {
                if (action->text() == PianoRollContextMenuController::tr("Edit Phonemes..."))
                    edit = action;
            }
            QVERIFY(edit);
            QVERIFY(edit->isEnabled());
            QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                              menu->actionGeometry(edit).center());
            selected = true;
        });
        const auto point = canvas->mapFromScene(QPointF(
            canvas->tickToSceneX(720), PianoRollCoord::keyIndexToCenterY(
                                           62, ClipEditorGlobal::noteHeight * canvas->scaleY())));
        QContextMenuEvent event(QContextMenuEvent::Mouse, point,
                                canvas->viewport()->mapToGlobal(point));
        chooseAction.start(0);
        QApplication::sendEvent(canvas->viewport(), &event);
        chooseAction.stop();
        QVERIFY(selected);
        dialog = qobject_cast<PhonemeEditorDialog *>(QApplication::activeModalWidget());
        QTRY_VERIFY(dialog && dialog->isVisible());
        dialog->activateWindow();
        QTRY_VERIFY(dialog->isActiveWindow());
        QCoreApplication::processEvents();
    };

    openEditor();
    if (QTest::currentTestFailed())
        return;
    auto *list = dialog->findChild<PhonemeNameListWidget *>();
    QVERIFY(list);
    QCOMPARE(list->count(), 2);
    auto *first = qobject_cast<PhonemeNameItemView *>(list->itemWidget(list->item(0)));
    auto *second = qobject_cast<PhonemeNameItemView *>(list->itemWidget(list->item(1)));
    QVERIFY(first);
    QVERIFY(second);
    QSignalSpy finished(dialog.data(), &QDialog::finished);
    enterNoteText(first->leName(), {});
    if (QTest::currentTestFailed())
        return;
    QTest::mouseClick(dialog->okButton(), Qt::LeftButton);
    QVERIFY(dialog->isVisible());
    QVERIFY(finished.isEmpty());
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(note->phonemeNameSeq().edited.isEmpty());

    enterNoteText(first->leName(), QStringLiteral("m"));
    if (QTest::currentTestFailed())
        return;
    QTest::mouseClick(first->cbIsOnset(), Qt::LeftButton);
    QTest::mouseClick(second->cbIsOnset(), Qt::LeftButton);
    QTest::mouseClick(dialog->okButton(), Qt::LeftButton);
    QTRY_VERIFY(!dialog->isVisible());
    QCOMPARE(finished.size(), 1);
    const auto edited = note->phonemeNameSeq().edited;
    QCOMPARE(edited.size(), 2);
    QCOMPARE(edited.at(0).name, QStringLiteral("m"));
    QVERIFY(edited.at(0).isOnset);
    QVERIFY(!edited.at(1).isOnset);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);

    openEditor();
    if (QTest::currentTestFailed())
        return;
    list = dialog->findChild<PhonemeNameListWidget *>();
    QVERIFY(list);
    first = qobject_cast<PhonemeNameItemView *>(list->itemWidget(list->item(0)));
    QVERIFY(first);
    QCOMPARE(first->leName()->text(), QStringLiteral("m"));
    enterNoteText(first->leName(), QStringLiteral("n"));
    if (QTest::currentTestFailed())
        return;
    const auto afterCommit = runtime.documentVersion();
    QTest::mouseClick(dialog->cancelButton(), Qt::LeftButton);
    QCOMPARE(runtime.documentVersion(), afterCommit);
    QCOMPARE(note->phonemeNameSeq().edited, edited);

    openEditor();
    if (QTest::currentTestFailed())
        return;
    auto *reset = noteDialogButton(dialog, PhonemeEditorDialog::tr("Reset Phones"));
    QVERIFY(reset);
    QVERIFY(reset->isEnabled());
    QTest::mouseClick(reset, Qt::LeftButton);
    QVERIFY(!reset->isEnabled());
    list = dialog->findChild<PhonemeNameListWidget *>();
    QVERIFY(list);
    first = qobject_cast<PhonemeNameItemView *>(list->itemWidget(list->item(0)));
    QVERIFY(first);
    QCOMPARE(first->leName()->text(), QStringLiteral("l"));
    QTest::mouseClick(dialog->okButton(), Qt::LeftButton);
    QVERIFY(note->phonemeNameSeq().edited.isEmpty());
    QCOMPARE(note->phonemeNameSeq().original, draft.phonemes.nameSeq.original);
    historyManager->undo();
    QCOMPARE(note->phonemeNameSeq().edited, edited);
    historyManager->undo();
    QVERIFY(note->phonemeNameSeq().edited.isEmpty());
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::lyricSearchNavigatesTheActualEditorAndHandlesNoMatches() {
    const auto nativeFrame = appOptions->appearance()->useNativeFrame;
    appOptions->appearance()->useNativeFrame = true;
    const auto restoreFrame =
        qScopeGuard([&] { appOptions->appearance()->useNativeFrame = nativeFrame; });
    MainWindow window;
    const auto clearWindow = qScopeGuard([&] {
        documentWorkflowController->setUi(nullptr);
        trackController->setParentWidget(nullptr);
        Dialog::setGlobalContext(nullptr);
        Toast::setGlobalContext(nullptr);
    });
    window.resize(1100, 800);
    window.show();
    window.activateWindow();
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    view->hide();
    singingClip->setLength(14000);
    singingClip->setClipLen(14000);
    auto &runtime = *context->m_coreRuntime;
    QList<Automation::NoteDraftDto> drafts;
    for (int index = 0; index < 3; ++index) {
        Automation::NoteDraftDto note;
        note.localStart = 2000 + index * 4000;
        note.length = 480;
        note.keyIndex = 60 + index * 4;
        note.lyric = index == 0 ? QStringLiteral("other") : QStringLiteral("find-%1").arg(index);
        note.language = QStringLiteral("eng");
        drafts.append(note);
    }
    QVERIFY(runtime.notes().insertNotes(commandContext(), Automation::ClipId(singingClip->id()),
                                        drafts));
    QList<int> expected;
    for (const auto *note : singingClip->notes()) {
        if (note->lyric().startsWith(QStringLiteral("find")))
            expected.append(note->id());
    }
    QCOMPARE(expected.size(), 2);
    QVERIFY(window.showBottomPanelPage(QStringLiteral("ClipEditor")));
    auto *editor = window.findChild<ClipEditorView *>();
    QVERIFY(editor);
    editor->onActiveClipChanged(singingClip->id());
    QTRY_VERIFY(editor->hasActiveSingingClip());
    QVERIFY(window.setPianoRollScale(2.0, 1.0));
    auto *pianoRoll = window.findChild<PianoRollGraphicsView *>();
    QVERIFY(pianoRoll);
    QTRY_VERIFY(pianoRoll->isVisible());
    historyManager->reset();
    const auto before = runtime.documentVersion();
    bool interacted = false;
    QTimer search;
    search.setInterval(10);
    connect(&search, &QTimer::timeout, &window, [&] {
        auto *dialog = qobject_cast<SearchDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        search.stop();
        const auto closeDialog = qScopeGuard([&] { dialog->reject(); });
        auto *input = dialog->findChild<QLineEdit *>();
        auto *results = dialog->findChild<QListWidget *>();
        auto *next = noteDialogButton(dialog, SearchDialog::tr("Next"));
        auto *previous = noteDialogButton(dialog, SearchDialog::tr("Previous"));
        QVERIFY(results);
        QVERIFY(next);
        QVERIFY(previous);
        enterNoteText(input, QStringLiteral("find"));
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(results->count(), 2);
        QCOMPARE(appStatus->selectedNotes.get(), QList<int>{expected.first()});
        const auto tickPerPixel = qAbs(pianoRoll->sceneXToTick(1) - pianoRoll->sceneXToTick(0));
        QTRY_VERIFY(qAbs(window.captureEditorViewState().pianoRoll.centerTick - 6000.0) <=
                    tickPerPixel);
        const auto firstCenter = window.captureEditorViewState().pianoRoll.centerTick;
        QTest::mouseClick(next, Qt::LeftButton);
        QCOMPARE(results->currentRow(), 1);
        QCOMPARE(appStatus->selectedNotes.get(), QList<int>{expected.last()});
        QTRY_VERIFY(window.captureEditorViewState().pianoRoll.centerTick > firstCenter + 3000);
        QTest::mouseClick(previous, Qt::LeftButton);
        QCOMPARE(appStatus->selectedNotes.get(), QList<int>{expected.first()});
        QTest::mouseClick(results->viewport(), Qt::LeftButton, Qt::NoModifier,
                          results->visualItemRect(results->item(1)).center());
        QCOMPARE(appStatus->selectedNotes.get(), QList<int>{expected.last()});
        enterNoteText(input, QStringLiteral("missing"));
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(results->count(), 0);
        QVERIFY(!next->isEnabled());
        QVERIFY(!previous->isEnabled());
        interacted = true;
    });
    search.start();
    clipController->onSearchLyric(&window);
    search.stop();
    QVERIFY(interacted);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::phonemeBoundaryDragCommitsAndUndoRestoresOffsets() {
    createLyricSelection();
    if (QTest::currentTestFailed())
        return;
    const auto notes = singingClip->notes().toList();
    QCOMPARE(notes.size(), 3);
    PhonemeView phonemes;
    phonemes.resize(960, 100);
    phonemes.setDataContext(singingClip);
    phonemes.setTimeRange(0, 1920);
    const auto detach = qScopeGuard([&] { phonemes.setDataContext(nullptr); });
    phonemes.show();
    phonemes.activateWindow();
    QTRY_VERIFY(phonemes.isVisible());
    auto &runtime = *context->m_coreRuntime;
    const auto phonemeTick = [&](const Note *note, const qsizetype index) {
        return qRound(appModel->msToTick(appModel->tickToMs(note->globalStart()) +
                                         note->phonemeOffsetSeq().result().at(index)));
    };
    const auto dragAndUndo = [&](Note *note, const qsizetype index, const int requestedTick,
                                 const int expectedTick) {
        const auto original = note->phonemeOffsetSeq();
        const auto names = note->phonemeNameSeq().result();
        QVERIFY(!original.isEdited());
        const auto press = QPoint(qRound(phonemeTick(note, index) * phonemes.width() / 1920.0), 50);
        const auto release = QPoint(qRound(requestedTick * phonemes.width() / 1920.0), 50);
        QVERIFY(phonemes.rect().contains(press));
        QVERIFY(phonemes.rect().contains(release));
        const auto before = runtime.documentVersion();
        const auto releaseOnFailure = qScopeGuard([&] {
            if (editSessionManager->hasActiveTransaction())
                QTest::mouseRelease(&phonemes, Qt::LeftButton, Qt::NoModifier, press);
        });
        QTest::mousePress(&phonemes, Qt::LeftButton, Qt::NoModifier, press);
        QVERIFY(editSessionManager->hasActiveTransaction());
        QCOMPARE(appStatus->currentEditObject.get(), AppStatus::EditObjectType::Phoneme);
        QMouseEvent move(QEvent::MouseMove, QPointF(release),
                         QPointF(phonemes.mapToGlobal(release)), Qt::NoButton, Qt::LeftButton,
                         Qt::NoModifier);
        QApplication::sendEvent(&phonemes, &move);
        QCOMPARE(note->phonemeOffsetSeq().result(), original.result());
        QCOMPARE(runtime.documentVersion(), before);
        QVERIFY(!historyManager->canUndo());
        QTest::mouseRelease(&phonemes, Qt::LeftButton, Qt::NoModifier, release);
        QVERIFY(!editSessionManager->hasActiveTransaction());
        QCOMPARE(appStatus->currentEditObject.get(), AppStatus::EditObjectType::None);
        const auto changed = note->phonemeOffsetSeq().edited;
        QCOMPARE(changed.size(), original.result().size());
        QVERIFY(qAbs(phonemeTick(note, index) - expectedTick) <= 2);
        for (qsizetype phone = 0; phone < changed.size(); ++phone) {
            if (phone != index)
                QCOMPARE(changed.at(phone), original.result().at(phone));
        }
        QCOMPARE(note->phonemeNameSeq().result(), names);
        QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
        QVERIFY(runtime.history().undo(commandContext()));
        QTRY_VERIFY(taskManager->tasks().isEmpty());
        QCOMPARE(note->phonemeOffsetSeq().result(), original.result());
        QVERIFY(!note->phonemeOffsetSeq().isEdited());
        QVERIFY(!historyManager->canUndo());
        QVERIFY(runtime.history().redo(commandContext()));
        QCOMPARE(note->phonemeOffsetSeq().edited, changed);
        QVERIFY(runtime.history().undo(commandContext()));
        QTRY_VERIFY(taskManager->tasks().isEmpty());
    };
    auto *middle = notes.at(1);
    auto *last = notes.last();
    QVERIFY(middle->phonemeOffsetSeq().result().size() >= 2);
    QVERIFY(last->phonemeOffsetSeq().result().size() >= 2);
    const auto vowel = middle->phonemeOffsetSeq().result().size() - 1;
    const auto start = phonemeTick(middle, vowel);
    dragAndUndo(middle, vowel, start + 60, start + 60);
    if (QTest::currentTestFailed())
        return;
    const auto leftBoundary = phonemeTick(middle, vowel - 1);
    dragAndUndo(middle, vowel, qMax(0, leftBoundary - 120), leftBoundary);
    if (QTest::currentTestFailed())
        return;
    const auto middleEnd = middle->globalStart() + middle->length();
    const auto rightBoundary = qMin(phonemeTick(last, 0), middleEnd);
    dragAndUndo(middle, vowel, middleEnd + 120, rightBoundary);
    if (QTest::currentTestFailed())
        return;
    const auto lastEnd = last->globalStart() + last->length();
    dragAndUndo(last, last->phonemeOffsetSeq().result().size() - 1, lastEnd + 120, lastEnd);
}

void ApplicationGuiTests::phonemeWaveformsLoadAndDiscardResultsAfterChangingClips() {
    createLyricSelection();
    if (QTest::currentTestFailed())
        return;
    inferController->startPendingAcousticInference();
    QTRY_VERIFY_WITH_TIMEOUT(std::all_of(singingClip->pieces().cbegin(),
                                         singingClip->pieces().cend(),
                                         [](const InferPiece *piece) {
                                             return piece->acousticInferStatus == Success;
                                         }) &&
                                 taskManager->tasks().isEmpty(),
                             15000);
    const auto pieces = singingClip->pieces();
    QVERIFY(!pieces.isEmpty());
    QVERIFY(!pieces.first()->audioPath.isEmpty());
    const auto pieceId = pieces.first()->id();
    const auto before = context->m_coreRuntime->documentVersion();
    PhonemeView phonemes;
    phonemes.resize(960, 100);
    phonemes.setTimeRange(0, 1920);
    QSignalSpy loaded(&phonemes, &PhonemeView::waveformReady);
    phonemes.setDataContext(singingClip);
    const auto detach = qScopeGuard([&] { phonemes.setDataContext(nullptr); });
    phonemes.show();
    QTRY_VERIFY_WITH_TIMEOUT(std::any_of(loaded.cbegin(), loaded.cend(),
                                         [pieceId](const auto &arguments) {
                                             return arguments.first().toInt() == pieceId;
                                         }),
                             10000);
    auto *pool = QThreadPool::globalInstance();
    QVERIFY(pool->waitForDone(5000));
    QCoreApplication::sendPostedEvents(&phonemes, QEvent::MetaCall);
    const auto completedLoads = loaded.count();
    QCOMPARE(context->m_coreRuntime->documentVersion(), before);
    phonemes.setDataContext(nullptr);

    const auto previousMaximum = pool->maxThreadCount();
    QSemaphore workerStarted;
    QSemaphore releaseWorker;
    pool->setMaxThreadCount(1);
    const auto restorePool = qScopeGuard([&] {
        releaseWorker.release();
        pool->waitForDone();
        pool->setMaxThreadCount(previousMaximum);
    });
    pool->start([&] {
        workerStarted.release();
        releaseWorker.acquire();
    });
    QVERIFY(workerStarted.tryAcquire(1, 5000));
    phonemes.setDataContext(singingClip);
    phonemes.setDataContext(nullptr);
    releaseWorker.release();
    QVERIFY(pool->waitForDone(5000));
    QCoreApplication::sendPostedEvents(&phonemes, QEvent::MetaCall);
    QCOMPARE(loaded.count(), completedLoads);
    QCOMPARE(context->m_coreRuntime->documentVersion(), before);
    QVERIFY(!editSessionManager->hasActiveTransaction());
}

void ApplicationGuiTests::movingLyricsBackwardUsesTheSelectedWordRange_data() {
    QTest::addColumn<bool>("contiguous");
    QTest::newRow("shift-complete-word-bundles") << true;
    QTest::newRow("noncontiguous-selection-is-disabled") << false;
}

void ApplicationGuiTests::movingLyricsBackwardUsesTheSelectedWordRange() {
    QFETCH(bool, contiguous);
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    QList<Automation::NoteDraftDto> drafts;
    for (int index = 0; index < 5; ++index) {
        Automation::NoteDraftDto draft;
        draft.localStart = index * 480;
        draft.length = 480;
        draft.keyIndex = 60;
        draft.lyric = QStringLiteral("word-%1").arg(index);
        draft.language = index % 2 == 0 ? QStringLiteral("cmn") : QStringLiteral("eng");
        draft.pronunciation.original = QStringLiteral("original-%1").arg(index);
        draft.pronunciation.edited = QStringLiteral("edited-%1").arg(index);
        draft.pronunciationCandidates = {QStringLiteral("candidate-%1").arg(index)};
        PhonemeName consonant;
        consonant.language = draft.language;
        consonant.name = QStringLiteral("l");
        PhonemeName vowel;
        vowel.language = draft.language;
        vowel.name = QStringLiteral("a");
        vowel.isOnset = true;
        draft.phonemes.nameSeq.original = {consonant, vowel};
        consonant.name = QStringLiteral("m");
        draft.phonemes.nameSeq.edited = {consonant, vowel};
        draft.phonemes.offsetSeq.original = {0, 80};
        draft.phonemes.offsetSeq.edited = {0, 60};
        drafts.append(draft);
    }
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.notes().insertNotes(commandContext(), Automation::ClipId(singingClip->id()),
                                        drafts));
    const auto notes = singingClip->notes().toList();
    QCOMPARE(notes.size(), 5);
    view->hide();
    PianoRollView pianoRoll;
    pianoRoll.setDataContext(singingClip);
    const auto detach = qScopeGuard([&] { pianoRoll.setDataContext(nullptr); });
    pianoRoll.resize(960, 600);
    pianoRoll.show();
    pianoRoll.activateWindow();
    QVERIFY(pianoRoll.setViewScale(1.0, 1.0));
    QVERIFY(pianoRoll.centerAt(1920, 60));
    auto *canvas = pianoRoll.findChild<PianoRollGraphicsView *>();
    QVERIFY(canvas);
    QTRY_VERIFY(pianoRoll.isActiveWindow());
    appStatus->selectedNotes = QList<int>{notes.at(contiguous ? 2 : 3)->id(), notes.at(1)->id()};
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto beforeModel = context->m_appModel->serialize();
    bool inspectedMenu = false;
    QTimer chooseAction;
    chooseAction.setSingleShot(true);
    connect(&chooseAction, &QTimer::timeout, &pianoRoll, [&] {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        QVERIFY(menu);
        const auto close = qScopeGuard([&] { menu->close(); });
        QAction *shift = nullptr;
        for (auto *action : menu->actions()) {
            if (action->text() == PianoRollContextMenuController::tr("Move Lyrics Backward"))
                shift = action;
        }
        QVERIFY(shift);
        QCOMPARE(shift->isEnabled(), contiguous);
        inspectedMenu = true;
        if (contiguous)
            QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                              menu->actionGeometry(shift).center());
    });
    const auto point = canvas->mapFromScene(QPointF(
        canvas->tickToSceneX(720),
        PianoRollCoord::keyIndexToCenterY(60, ClipEditorGlobal::noteHeight * canvas->scaleY())));
    QContextMenuEvent event(QContextMenuEvent::Mouse, point,
                            canvas->viewport()->mapToGlobal(point));
    chooseAction.start(0);
    QApplication::sendEvent(canvas->viewport(), &event);
    chooseAction.stop();
    QVERIFY(inspectedMenu);
    if (!contiguous) {
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(context->m_appModel->serialize(), beforeModel);
        QVERIFY(!historyManager->canUndo());
        return;
    }
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QCOMPARE(notes.first()->lyric(), drafts.first().lyric);
    QCOMPARE(notes.first()->phonemes().nameSeq.edited, drafts.first().phonemes.nameSeq.edited);
    QCOMPARE(notes.at(1)->lyric(), QStringLiteral("-"));
    QCOMPARE(notes.at(2)->lyric(), QStringLiteral("-"));
    for (int index = 1; index < notes.size(); ++index) {
        const auto *note = notes.at(index);
        QVERIFY(!note->phonemeNameSeq().isEdited());
        QVERIFY(!note->phonemeOffsetSeq().isEdited());
        if (index >= 3) {
            const auto &source = drafts.at(index - 2);
            QCOMPARE(note->lyric(), source.lyric);
            QCOMPARE(note->language(), source.language);
            // G2P regenerates the original pronunciation; word edits carry the manual override.
            QCOMPARE(note->pronunciation().result(), source.pronunciation.result());
            QCOMPARE(note->pronunciation().edited, source.pronunciation.edited);
            QCOMPARE(note->pronCandidates(), source.pronunciationCandidates);
        } else {
            QVERIFY(note->pronunciation().edited.isEmpty());
            QVERIFY(note->pronCandidates().isEmpty());
        }
    }
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(context->m_appModel->serialize(), beforeModel);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(runtime.history().redo(commandContext()));
    QCOMPARE(notes.last()->lyric(), drafts.at(2).lyric);
}
