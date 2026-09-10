#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/TrackController.h"
#include "UI/Controls/TrackColorSwatchWidget.h"
#include "UI/Views/TrackEditor/TrackControlView.h"
#include "UI/Views/TrackEditor/TrackEditorView.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/InlineEditLabel.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest/QTest>

void ApplicationGuiTests::trackHeaderInputsCommitAndUndo() {
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    const auto clearDialogParent = qScopeGuard([] { trackController->setParentWidget(nullptr); });
    TrackEditorView editor;
    Automation::TrackDraftDto draft;
    draft.name = QStringLiteral("Original lead");
    draft.defaultLanguage = QStringLiteral("eng");
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, draft));
    auto *track = context->m_appModel->tracks().first();
    auto *controls = editor.findChild<TrackControlView *>();
    QVERIFY(controls);
    auto *mute = controls->findChild<Button *>("btnMute");
    auto *solo = controls->findChild<Button *>("btnSolo");
    auto *name = controls->findChild<InlineEditLabel *>("leTrackName");
    QVERIFY(mute && solo && name);
    editor.resize(1100, 500);
    editor.show();
    editor.activateWindow();
    QTRY_VERIFY(editor.isActiveWindow() && name->isVisible());
    historyManager->reset();
    const auto before = runtime.documentVersion();

    QTest::mouseClick(mute, Qt::LeftButton);
    QVERIFY(track->control().mute());
    QVERIFY(!track->control().solo());
    QTest::mouseClick(solo, Qt::LeftButton);
    QVERIFY(track->control().mute() && track->control().solo());
    QCOMPARE(runtime.documentVersion().revision, before.revision + 2);

    QTest::mouseDClick(name, Qt::LeftButton);
    QTRY_VERIFY(qobject_cast<QLineEdit *>(QApplication::focusWidget()));
    auto *text = qobject_cast<QLineEdit *>(QApplication::focusWidget());
    QTest::keySequence(text, QKeySequence::SelectAll);
    QTest::keyClicks(text, "Discarded name");
    QTest::keyClick(text, Qt::Key_Escape);
    QCOMPARE(track->name(), draft.name);
    QCOMPARE(name->text(), draft.name);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 2);

    QTest::mouseDClick(name, Qt::LeftButton);
    QTRY_VERIFY(qobject_cast<QLineEdit *>(QApplication::focusWidget()));
    text = qobject_cast<QLineEdit *>(QApplication::focusWidget());
    QTest::keySequence(text, QKeySequence::SelectAll);
    QTest::keyClicks(text, "Edited lead");
    QTest::keyClick(text, Qt::Key_Return);
    QCOMPARE(track->name(), QStringLiteral("Edited lead"));
    QCOMPARE(name->text(), track->name());
    QCOMPARE(runtime.documentVersion().revision, before.revision + 3);
    QVERIFY(track->control().mute() && track->control().solo());

    historyManager->undo();
    QCOMPARE(track->name(), draft.name);
    QCOMPARE(name->text(), draft.name);
    historyManager->undo();
    QVERIFY(track->control().mute());
    QVERIFY(!track->control().solo());
    QVERIFY(mute->isChecked() && !solo->isChecked());
    historyManager->undo();
    QVERIFY(!track->control().mute() && !track->control().solo());
    QVERIFY(!mute->isChecked() && !solo->isChecked());
    QVERIFY(!historyManager->canUndo());
    historyManager->redo();
    QVERIFY(track->control().mute() && mute->isChecked());
}

void ApplicationGuiTests::trackColorMenuPreviewsAndCommits_data() {
    QTest::addColumn<bool>("commit");
    QTest::newRow("escape-restores-preview") << false;
    QTest::newRow("click-commits-color") << true;
}

void ApplicationGuiTests::trackColorMenuPreviewsAndCommits() {
    QFETCH(bool, commit);
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    const auto clearDialogParent = qScopeGuard([] { trackController->setParentWidget(nullptr); });
    TrackEditorView editor;
    Automation::TrackDraftDto draft;
    draft.name = QStringLiteral("Color preview");
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, draft));
    auto *track = context->m_appModel->tracks().first();
    auto *controls = editor.findChild<TrackControlView *>();
    QVERIFY(controls);
    editor.resize(1100, 500);
    editor.show();
    editor.activateWindow();
    QTRY_VERIFY(editor.isActiveWindow() && controls->isVisible());
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto originalColor = track->colorIndex();
    int chosenColor = -1;
    bool previewObserved = false;
    QTimer::singleShot(0, &editor, [&] {
        const auto closeMenus = qScopeGuard([&] {
            for (auto *menu : controls->findChildren<QMenu *>())
                menu->close();
        });
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        QVERIFY(menu);
        QAction *colorAction = nullptr;
        for (auto *action : menu->actions()) {
            if (action->text() == TrackControlView::tr("Track color"))
                colorAction = action;
        }
        QVERIFY(colorAction && colorAction->menu());
        QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                          menu->actionGeometry(colorAction).center());
        auto *colors = colorAction->menu();
        QTRY_VERIFY(colors->isVisible());
        auto *swatch = colors->findChild<TrackColorSwatchWidget *>();
        QVERIFY(swatch);
        QSignalSpy previews(swatch, &TrackColorSwatchWidget::colorIndexHovered);
        const auto column = originalColor == 1 ? 2 : 1;
        const QPoint point(swatch->width() * (column * 2 + 1) / 8, swatch->height() / 6);
        QTest::mouseMove(swatch, point);
        QTRY_VERIFY(!previews.isEmpty());
        chosenColor = previews.last().first().toInt();
        QVERIFY(chosenColor != originalColor);
        QCOMPARE(track->colorIndex(), chosenColor);
        QCOMPARE(runtime.documentVersion(), before);
        QVERIFY(!historyManager->canUndo());
        previewObserved = true;
        if (commit) {
            QTest::mouseClick(swatch, Qt::LeftButton, Qt::NoModifier, point);
        } else {
            QTest::keyClick(colors, Qt::Key_Escape);
            QTest::keyClick(menu, Qt::Key_Escape);
        }
    });
    const auto point = controls->rect().center();
    QContextMenuEvent menuEvent(QContextMenuEvent::Mouse, point, controls->mapToGlobal(point));
    QApplication::sendEvent(controls, &menuEvent);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(previewObserved);
    QCOMPARE(track->colorIndex(), commit ? chosenColor : originalColor);
    if (commit) {
        QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
        historyManager->undo();
        QCOMPARE(track->colorIndex(), originalColor);
        QVERIFY(!historyManager->canUndo());
        historyManager->redo();
        QCOMPARE(track->colorIndex(), chosenColor);
    } else {
        QCOMPARE(runtime.documentVersion(), before);
        QVERIFY(!historyManager->canUndo());
    }
}
