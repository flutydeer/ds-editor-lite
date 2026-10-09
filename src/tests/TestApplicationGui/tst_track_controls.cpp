#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/TrackController.h"
#include "UI/Controls/LevelMeterManager.h"
#include "UI/Controls/TrackColorSwatchWidget.h"
#include "UI/Views/TrackEditor/TrackControlView.h"
#include "UI/Views/TrackEditor/TrackEditorView.h"
#include "UI/Views/TrackEditor/TrackListView.h"
#include "UI/Views/TrackEditor/TracksGraphicsView.h"
#include "UI/Views/TrackEditor/InfoLane/TempoLaneView.h"
#include "UI/Views/TrackEditor/InfoLane/TimeSignatureLaneView.h"
#include "UI/Views/MixConsole/MixConsoleView.h"
#include "UI/Views/MixConsole/ChannelView.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/InlineEditLabel.h>
#include <lite/GUI/Controls/LevelMeter.h>
#include <lite/GUI/Controls/LevelMeterViewModel.h>
#include <lite/GUI/Controls/OverlaySplitter.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QApplication>
#include <QInputMethodEvent>
#include <QLocale>
#include <QContextMenuEvent>
#include <QCursor>
#include <QHoverEvent>
#include <QImage>
#include <QPixmap>
#include <QKeySequence>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTimer>
#include <QWindow>
#include <QWheelEvent>
#include <QScrollBar>
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
    ThemeManager::instance()->addStyleRoot(&console);
    const auto restoreStyle =
        qScopeGuard([&] { ThemeManager::instance()->removeStyleRoot(&console); });
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
    if (!master) {
        auto *list = console.findChild<QListWidget *>();
        QVERIFY(list);
        Automation::TrackDraftDto neighbor;
        neighbor.name = QStringLiteral("Neighbor channel");
        QVERIFY(runtime.project().insertTrack(commandContext(), 1, neighbor));
        QCOMPARE(list->count(), 2);
        auto *neighborChannel = qobject_cast<ChannelView *>(list->itemWidget(list->item(1)));
        QVERIFY(neighborChannel && neighborChannel != channel);
        auto *index = channel->findChild<QLabel *>("lbIndex");
        auto *neighborIndex = neighborChannel->findChild<QLabel *>("lbIndex");
        QVERIFY(index && neighborIndex);
        const auto moved =
            runtime.project().moveTrack(commandContext(), Automation::TrackId(track->id()), 2);
        QVERIFY(moved && moved.get().changed);
        QCOMPARE(context->m_appModel->tracks().last(), track);
        auto *meters = AppContext::instance<LevelMeterManager>();
        QVERIFY(meters);
        QCOMPARE(list->itemWidget(list->item(1)), channel);
        QCOMPARE(list->itemWidget(list->item(0)), neighborChannel);
        QCOMPARE(channel->levelMeter()->viewModel(), meters->viewModelAt(1));
        QCOMPARE(neighborChannel->levelMeter()->viewModel(), meters->viewModelAt(0));
        QCOMPARE(index->text(), QLocale().toString(2));
        QCOMPARE(neighborIndex->text(), QLocale().toString(1));
        QVERIFY(runtime.history().undo(commandContext()));
        QCOMPARE(list->itemWidget(list->item(0)), channel);
        QCOMPARE(channel->levelMeter()->viewModel(), meters->viewModelAt(0));
        QCOMPARE(index->text(), QLocale().toString(1));
        QCOMPARE(neighborIndex->text(), QLocale().toString(2));
        QVERIFY(runtime.history().redo(commandContext()));
        QCOMPARE(list->itemWidget(list->item(1)), channel);
        QCOMPARE(&channel->context(), track);
    }
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
        QTest::keySequence(input, QKeySequence::SelectAll);
        QInputMethodEvent textInput;
        textInput.setCommitString(value);
        QApplication::sendEvent(input, &textInput);
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
    enter(gain, QStringLiteral("-∞"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(control().gain(), -54.0);
    QCOMPARE(gain->text(), QStringLiteral("-∞"));
    enter(gain, QLocale().toString(-6.0, 'f', 1));
    QCOMPARE(control().gain(), -6.0);
    enter(pan, QLocale().toString(-100));
    QCOMPARE(control().pan(), -1.0);
    QCOMPARE(pan->text(), QStringLiteral("L100"));
    enter(pan, QLocale().toString(100));
    QCOMPARE(control().pan(), 1.0);
    QCOMPARE(pan->text(), QStringLiteral("R100"));
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
    QCOMPARE(applied.revision, before.revision + 9);
    const auto *panEdit = historyManager->nextUndoEntry();
    enter(pan, QStringLiteral("invalid"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(runtime.documentVersion(), applied);
    QCOMPARE(historyManager->nextUndoEntry(), panEdit);
    QCOMPARE(control().pan(), -0.1);
    QCOMPARE(pan->text(), QStringLiteral("L10"));
    for (int i = 0; i < 9; ++i)
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
    QTRY_VERIFY(levels->displayedPeakL() > 0.01 && levels->displayedPeakR() > 0.01);
    QVERIFY(meter->property("showValueWhenHover").toBool());
    const auto hover = [&](QEvent::Type type, QPoint position) {
        QHoverEvent event(type, position, meter->mapToGlobal(position), QPointF{});
        QApplication::sendEvent(meter, &event);
    };
    hover(QEvent::HoverLeave, {-1, -1});
    const auto withoutReadout = meter->grab().toImage();
    QVERIFY(!withoutReadout.isNull());
    const QPoint upper(meter->width() / 2, meter->height() / 3);
    const QPoint lower(meter->width() / 2, meter->height() * 2 / 3);
    hover(QEvent::HoverEnter, upper);
    const auto upperReadout = meter->grab().toImage();
    QVERIFY(upperReadout != withoutReadout);
    hover(QEvent::HoverMove, lower);
    QVERIFY(meter->grab().toImage() != upperReadout);
    const auto indicatorY = meter->property("padding").toDouble() +
                            meter->property("clipIndicatorLength").toDouble() / 2;
    hover(QEvent::HoverMove, {meter->width() / 2, qRound(indicatorY)});
    QCOMPARE(meter->grab().toImage(), withoutReadout);
    hover(QEvent::HoverMove, upper);
    QVERIFY(meter->grab().toImage() != withoutReadout);
    hover(QEvent::HoverLeave, {-1, -1});
    QCOMPARE(meter->grab().toImage(), withoutReadout);
    QVERIFY(levels->clippedL() && levels->clippedR());
    QTest::mouseClick(meter, Qt::LeftButton, Qt::NoModifier, QPoint(meter->width() / 2, 10));
    QVERIFY(!levels->clippedL() && !levels->clippedR());
    QVERIFY(otherLevels->clippedL() && otherLevels->clippedR());
    QCOMPARE(runtime.documentVersion(), beforeMeterInput);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::trackHeaderAndInfoLaneWheelsKeepTheCanvasAligned_data() {
    QTest::addColumn<QString>("origin");
    QTest::newRow("track-header") << QStringLiteral("header");
    QTest::newRow("track-control") << QStringLiteral("control");
    QTest::newRow("track-index") << QStringLiteral("index");
    QTest::newRow("track-meter") << QStringLiteral("level");
    QTest::newRow("tempo-lane") << QStringLiteral("tempo");
    QTest::newRow("time-signature-lane") << QStringLiteral("meter");
}

void ApplicationGuiTests::trackHeaderAndInfoLaneWheelsKeepTheCanvasAligned() {
    QFETCH(QString, origin);
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    const auto clearDialogParent = qScopeGuard([] { trackController->setParentWidget(nullptr); });
    TrackEditorView editor;
    for (int index = 0; index < 12; ++index) {
        Automation::TrackDraftDto draft;
        draft.name = QStringLiteral("Track %1").arg(index + 1);
        QVERIFY(runtime.project().insertTrack(commandContext(), index, draft));
    }
    auto *canvas = editor.findChild<TracksGraphicsView *>();
    auto *headers = editor.findChild<TrackListView *>();
    QVERIFY(canvas && headers);
    canvas->setAnimationEnabled(false);
    editor.resize(1100, 500);
    editor.show();
    editor.activateWindow();
    QTRY_VERIFY(editor.isActiveWindow());
    QVERIFY(editor.setViewScale(1.0, 1.0));
    QVERIFY(editor.centerAt(9600, 5));
    const auto headerInput = origin != QStringLiteral("tempo") && origin != QStringLiteral("meter");
    QWidget *target = headers->viewport();
    if (origin == QStringLiteral("control") || origin == QStringLiteral("index") ||
        origin == QStringLiteral("level")) {
        auto *item = headers->itemAt(headers->viewport()->rect().center());
        QVERIFY(item);
        auto *controls = qobject_cast<TrackControlView *>(headers->itemWidget(item));
        QVERIFY(controls);
        target = controls;
        if (origin == QStringLiteral("index"))
            target = controls->findChild<QLabel *>("lbTrackIndex");
        else if (origin == QStringLiteral("level"))
            target = controls->levelMeter();
    } else if (origin == QStringLiteral("tempo"))
        target = editor.findChild<TempoLaneView *>();
    else if (origin == QStringLiteral("meter"))
        target = editor.findChild<TimeSignatureLaneView *>();
    QVERIFY(target && target->isVisible());
    historyManager->reset();
    const auto document = runtime.documentVersion();
    const auto model = TestSupport::projectSnapshot(*context->m_appModel);
    const auto wheel = [&](int delta, Qt::KeyboardModifiers modifiers) {
        const auto position = target->rect().center();
        QWheelEvent event(QPointF(position), QPointF(target->mapToGlobal(position)), {}, {0, delta},
                          Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
        QApplication::sendEvent(target, &event);
    };
    const auto beforeScroll = editor.viewState();
    wheel(-120, Qt::NoModifier);
    QTRY_VERIFY(editor.viewState().centerTrackIndex > beforeScroll.centerTrackIndex);
    QTRY_COMPARE(headers->verticalScrollBar()->value(), canvas->verticalScrollBar()->value());
    QCOMPARE(editor.viewState().verticalScale, beforeScroll.verticalScale);
    const auto beforeScale = editor.viewState();
    const auto rowHeight = headers->visualItemRect(headers->item(0)).height();
    wheel(120, Qt::AltModifier);
    QTRY_VERIFY(editor.viewState().verticalScale > beforeScale.verticalScale);
    QTRY_VERIFY(headers->visualItemRect(headers->item(0)).height() > rowHeight);
    QTRY_COMPARE(headers->verticalScrollBar()->value(), canvas->verticalScrollBar()->value());
    if (origin == QStringLiteral("header")) {
        auto *controls = qobject_cast<TrackControlView *>(headers->itemWidget(headers->item(0)));
        QVERIFY(controls);
        auto *singer = controls->findChild<QWidget *>("cbSinger");
        auto *language = controls->findChild<QWidget *>("cbLanguage");
        QVERIFY(singer && language);
        QVERIFY(!singer->isHidden() && !language->isHidden());
        wheel(-240, Qt::AltModifier);
        QTRY_VERIFY(editor.viewState().verticalScale < beforeScale.verticalScale);
        QTRY_VERIFY(singer->isHidden() && language->isHidden());
        QTRY_COMPARE(headers->verticalScrollBar()->value(), canvas->verticalScrollBar()->value());
        wheel(240, Qt::AltModifier);
        QTRY_VERIFY(!singer->isHidden() && !language->isHidden());
        QTRY_COMPARE(headers->verticalScrollBar()->value(), canvas->verticalScrollBar()->value());
    }
    if (!headerInput) {
        const auto beforeHorizontalScale = editor.viewState();
        wheel(120, Qt::ControlModifier);
        QTRY_VERIFY(editor.viewState().horizontalScale > beforeHorizontalScale.horizontalScale);
        const auto beforeHorizontalScroll = editor.viewState();
        wheel(-120, Qt::ShiftModifier);
        QTRY_VERIFY(editor.viewState().centerTick > beforeHorizontalScroll.centerTick);
        QCOMPARE(editor.viewState().horizontalScale, beforeHorizontalScroll.horizontalScale);
    }
    QCOMPARE(runtime.documentVersion(), document);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), model);
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

    const auto originalModel = TestSupport::projectSnapshot(*context->m_appModel);
    auto *splitter = editor.findChild<OverlaySplitter *>("trackSplitter");
    auto *grip = editor.findChild<SplitterOverlayGrip *>();
    QVERIFY(splitter && grip && grip->isVisible());
    const auto originalSizes = splitter->sizes();
    const auto dragBy = [&](int distance) {
        const auto local = grip->rect().center();
        const auto global = grip->mapToGlobal(local);
        QTest::mousePress(grip, Qt::LeftButton, Qt::NoModifier, local);
        const QPoint delta(distance, 0);
        QMouseEvent move(QEvent::MouseMove, QPointF(local + delta), QPointF(global + delta),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(grip, &move);
        QTest::mouseRelease(grip, Qt::LeftButton);
    };
    auto *panel = splitter->widget(0);
    dragBy(-splitter->width());
    QTRY_COMPARE(panel->width(), panel->minimumWidth());
    QVERIFY(controls->isVisible() && splitter->widget(1)->isVisible());
    dragBy(splitter->width());
    QTRY_COMPARE(panel->width(), panel->maximumWidth());
    dragBy(originalSizes.first() - splitter->sizes().first());
    QTRY_COMPARE(splitter->sizes(), originalSizes);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), originalModel);
    QVERIFY(!historyManager->canUndo());

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
