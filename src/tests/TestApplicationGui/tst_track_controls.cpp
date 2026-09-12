#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/TrackController.h"
#include "UI/Controls/TrackColorSwatchWidget.h"
#include "UI/Views/TrackEditor/TrackControlView.h"
#include "UI/Views/TrackEditor/TrackEditorView.h"
#include "UI/Views/MixConsole/MixConsoleView.h"
#include "UI/Views/MixConsole/ChannelView.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/InlineEditLabel.h>
#include <lite/GUI/Controls/LevelMeter.h>
#include <lite/GUI/Controls/LevelMeterViewModel.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QApplication>
#include <QClipboard>
#include <QLocale>
#include <QContextMenuEvent>
#include <QCursor>
#include <QKeySequence>
#include <QLineEdit>
#include <QLabel>
#include <QMenu>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTimer>
#include <QWindow>
#include <QtTest/QTest>

void ApplicationGuiTests::mixerChannelInputsAndLevelsStayScoped_data() {
    QTest::addColumn<bool>("master");
    QTest::newRow("track") << false;
    QTest::newRow("master") << true;
}

void ApplicationGuiTests::mixerChannelInputsAndLevelsStayScoped() {
    QFETCH(bool, master);
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    Automation::TrackDraftDto draft;
    draft.name = QStringLiteral("Mixer channel");
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, draft));
    auto *track = context->m_appModel->tracks().first();
    MixConsoleView console;
    console.resize(720, 520);
    console.show();
    console.activateWindow();
    QTRY_VERIFY(console.isActiveWindow());
    ChannelView *channel = nullptr;
    ChannelView *otherChannel = nullptr;
    for (auto *candidate : console.findChildren<ChannelView *>()) {
        if (candidate->isMasterChannel() == master)
            channel = candidate;
        else
            otherChannel = candidate;
    }
    QVERIFY(channel && otherChannel);
    auto *gain = channel->findChild<InlineEditLabel *>("elGain");
    auto *pan = channel->findChild<InlineEditLabel *>("elPan");
    QVERIFY(gain && pan);
    QTRY_VERIFY(gain->isVisible() && pan->isVisible());
    const auto control = [&] {
        return master ? context->m_appModel->masterControl() : track->control();
    };
    const auto initial = control();
    const auto other = master ? track->control() : context->m_appModel->masterControl();
    const auto enter = [&](InlineEditLabel *label, const QString &value,
                           Qt::Key finish = Qt::Key_Return) {
        QTest::mouseDClick(label, Qt::LeftButton);
        QTRY_VERIFY(qobject_cast<QLineEdit *>(QApplication::focusWidget()));
        auto *input = qobject_cast<QLineEdit *>(QApplication::focusWidget());
        QApplication::clipboard()->setText(value);
        QTest::keySequence(input, QKeySequence::SelectAll);
        QTest::keySequence(input, QKeySequence::Paste);
        QTest::keyClick(input, finish);
    };
    historyManager->reset();
    const auto before = runtime.documentVersion();
    enter(gain, QLocale().toString(-6.0, 'f', 1));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(control().gain(), -6.0);
    QCOMPARE(gain->text(), QLocale().toString(-6.0, 'f', 1));
    const auto *gainEdit = historyManager->nextUndoEntry();
    enter(gain, QStringLiteral("invalid"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(control().gain(), -6.0);
    QCOMPARE(gain->text(), QLocale().toString(-6.0, 'f', 1));
    QCOMPARE(historyManager->nextUndoEntry(), gainEdit);
    enter(gain, QLocale().toString(-12.0, 'f', 1), Qt::Key_Escape);
    QCOMPARE(control().gain(), -6.0);
    QCOMPARE(historyManager->nextUndoEntry(), gainEdit);
    enter(pan, QStringLiteral("L25"));
    QCOMPARE(control().pan(), -0.25);
    QCOMPARE(pan->text(), QStringLiteral("L25"));
    enter(pan, QStringLiteral("R40"));
    QCOMPARE(control().pan(), 0.4);
    QCOMPARE(pan->text(), QStringLiteral("R40"));
    enter(pan, QStringLiteral("C"));
    QCOMPARE(control().pan(), 0.0);
    enter(pan, QLocale().toString(-10));
    QCOMPARE(control().pan(), -0.1);
    QCOMPARE(pan->text(), QStringLiteral("L10"));
    const auto applied = runtime.documentVersion();
    QCOMPARE(applied.revision, before.revision + 5);
    const auto *panEdit = historyManager->nextUndoEntry();
    enter(pan, QStringLiteral("invalid"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(runtime.documentVersion(), applied);
    QCOMPARE(historyManager->nextUndoEntry(), panEdit);
    QCOMPARE(control().pan(), -0.1);
    QCOMPARE(pan->text(), QStringLiteral("L10"));
    for (int i = 0; i < 5; ++i)
        QVERIFY(runtime.history().undo(commandContext()));
    QVERIFY(!historyManager->canUndo());
    QCOMPARE(control().gain(), initial.gain());
    QCOMPARE(control().pan(), initial.pan());
    QCOMPARE(channel->control().gain(), initial.gain());
    QCOMPARE(channel->control().pan(), initial.pan());
    const auto untouched = master ? track->control() : context->m_appModel->masterControl();
    QCOMPARE(untouched.gain(), other.gain());
    QCOMPARE(untouched.pan(), other.pan());

    const auto beforeMeterInput = runtime.documentVersion();
    auto *meter = channel->levelMeter();
    auto *levels = meter->viewModel();
    auto *otherLevels = otherChannel->levelMeter()->viewModel();
    auto *peakLabel = channel->findChild<QLabel *>("lbPeakLevel");
    QVERIFY(levels && otherLevels && peakLabel);
    levels->setLevels(3, 6);
    otherLevels->setLevels(3, 3);
    QVERIFY(levels->clippedL() && levels->clippedR());
    QVERIFY(otherLevels->clippedL() && otherLevels->clippedR());
    QCOMPARE(peakLabel->text(), QStringLiteral("+") + QLocale().toString(6.0, 'f', 1));
    QTest::mouseClick(meter, Qt::LeftButton, Qt::NoModifier, QPoint(meter->width() / 2, 10));
    QVERIFY(!levels->clippedL() && !levels->clippedR());
    QVERIFY(otherLevels->clippedL() && otherLevels->clippedR());
    QCOMPARE(runtime.documentVersion(), beforeMeterInput);
    QVERIFY(!historyManager->canUndo());
}

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
    const auto previousCursor = QCursor::pos();
    const auto restoreCursor = qScopeGuard([&] { QCursor::setPos(previousCursor); });
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
    const auto menuPoint = controls->rect().center();
    QCursor::setPos(controls->mapToGlobal(menuPoint));
    QCoreApplication::processEvents();
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
        QVERIFY(colors->windowHandle());
        QTest::mouseMove(colors->windowHandle(), swatch->mapTo(colors, QPoint(0, 0)));
        QSignalSpy previews(swatch, &TrackColorSwatchWidget::colorIndexHovered);
        const auto column = originalColor == 1 ? 2 : 1;
        const QPoint point(swatch->width() * (column * 2 + 1) / 8, swatch->height() / 6);
        QTest::mouseMove(colors->windowHandle(), swatch->mapTo(colors, point));
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
    QContextMenuEvent menuEvent(QContextMenuEvent::Mouse, menuPoint,
                                controls->mapToGlobal(menuPoint));
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
