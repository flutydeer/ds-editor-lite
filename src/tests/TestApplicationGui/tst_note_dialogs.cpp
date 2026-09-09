#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/AppController.h"
#include "Controller/ClipController.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/TrackController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Dialogs/Note/PhonemeEditorDialog.h"
#include "UI/Dialogs/Note/PhonemeNameItemView.h"
#include "UI/Dialogs/Note/PhonemeNameListWidget.h"
#include "UI/Dialogs/Search/SearchDialog.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include "UI/Views/ClipEditor/ClipEditorView.h"
#include "UI/Window/MainWindow.h"

#include <lite/GUI/Controls/AccentButton.h>
#include <lite/GUI/Controls/LineEdit.h>
#include <lite/GUI/Controls/Toast.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest/QTest>

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
    historyManager->reset();
    const auto before = runtime.documentVersion();
    QPointer<PhonemeEditorDialog> dialog;

    const auto openEditor = [&] {
        bool selected = false;
        QTimer chooseAction;
        chooseAction.setSingleShot(true);
        connect(&chooseAction, &QTimer::timeout, view.get(), [&] {
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
        const auto point = pointFor(720, 62);
        QContextMenuEvent event(QContextMenuEvent::Mouse, point,
                                view->viewport()->mapToGlobal(point));
        chooseAction.start(0);
        QApplication::sendEvent(view->viewport(), &event);
        chooseAction.stop();
        QVERIFY(selected);
        dialog = qobject_cast<PhonemeEditorDialog *>(QApplication::activeModalWidget());
        QTRY_VERIFY(dialog && dialog->isVisible());
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
    const auto nativeFrame = appOptions->appearance()->useNativeFrame;
    appOptions->appearance()->useNativeFrame = true;
    const auto restoreFrame =
        qScopeGuard([&] { appOptions->appearance()->useNativeFrame = nativeFrame; });
    MainWindow window;
    const auto clearWindow = qScopeGuard([&] {
        documentWorkflowController->setUi(nullptr);
        appController->setMainWindow(nullptr);
        trackController->setParentWidget(nullptr);
        Dialog::setGlobalContext(nullptr);
        Toast::setGlobalContext(nullptr);
    });
    window.resize(1100, 800);
    window.show();
    window.activateWindow();
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
