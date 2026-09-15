#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/PlaybackController.h"
#include "Model/Utils/ParamUtils.h"
#include "UI/Dialogs/Base/MessageDialog.h"
#include "UI/Views/ClipEditor/ParamEditor/ParamEditorView.h"
#include "UI/Views/ClipEditor/ParamEditor/ParamEditorGraphicsView.h"
#include "UI/Views/ClipEditor/ParamEditor/SpeakerMixEditorView.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QApplication>
#include <QAbstractItemView>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest/QTest>

namespace {
    void showSpeakerMixPanel(ParamEditorView &panel, SingingClip *clip) {
        panel.setDataContext(clip);
        panel.resize(900, 360);
        panel.show();
        panel.activateWindow();
        auto *foreground = panel.findChild<ComboBox *>("cbForegroundParam");
        QVERIFY(foreground);
        const auto index = foreground->findText(paramUtils->nameFromType(ParamInfo::SpeakerMix));
        QVERIFY(index >= 0);
        QTest::mouseClick(foreground, Qt::LeftButton);
        QTRY_VERIFY(foreground->view()->isVisible());
        QTest::keyClick(foreground->view(), Qt::Key_Home);
        for (int row = 0; row < index; ++row)
            QTest::keyClick(foreground->view(), Qt::Key_Down);
        QTest::keyClick(foreground->view(), Qt::Key_Return);
        QCOMPARE(foreground->currentIndex(), index);
        QCOMPARE(panel.viewState().foreground, ParamInfo::SpeakerMix);
    }

    QPoint speakerMixPoint(ParamEditorGraphicsView &graphics, int tick, double heightRatio) {
        const auto visible = graphics.visibleRect();
        const auto ratio =
            (tick - graphics.startTick()) / (graphics.endTick() - graphics.startTick());
        auto *mix = graphics.speakerMixView();
        const auto y = mix->mapToScene(QPointF(0, mix->rect().height() * heightRatio)).y();
        return graphics.mapFromScene(QPointF(visible.left() + ratio * visible.width(), y));
    }

    void movePressedMouse(QWidget &viewport, const QPoint &position) {
        QMouseEvent move(QEvent::MouseMove, QPointF(position),
                         QPointF(viewport.mapToGlobal(position)), Qt::NoButton, Qt::LeftButton,
                         Qt::NoModifier);
        QApplication::sendEvent(&viewport, &move);
    }

    void seedDynamicMix(Automation::CoreRuntime &runtime, const Automation::CommandContext &command,
                        SingingClip &clip) {
        const SpeakerInfo bright(QStringLiteral("bright"), QStringLiteral("Bright"));
        const SpeakerInfo warm(QStringLiteral("warm"), QStringLiteral("Warm"));
        const SingerInfo singer(
            {QStringLiteral("mix"), QStringLiteral("gui"), QVersionNumber(1, 0)},
            QStringLiteral("Mix"), {bright, warm});
        SpeakerMixModel::SpeakerMixData mix;
        mix.mode = SpeakerMixModel::SingerSourceMode::DynamicMix;
        mix.sources = {{bright}, {warm}};
        mix.fixedWeights = {0.5};
        mix.dynamicKeyframes = {
            {0,   {0.5} },
            {480, {0.25}},
            {960, {0.75}}
        };
        QVERIFY(runtime.parameters().applyClipSpeakerMix(command, Automation::ClipId(clip.id()),
                                                         singer, bright, mix));
    }
}

void ApplicationGuiTests::dynamicSpeakerMixGesturesPreserveIdentityAndUndo() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    const SpeakerInfo bright(QStringLiteral("bright"), QStringLiteral("Bright"));
    const SpeakerInfo warm(QStringLiteral("warm"), QStringLiteral("Warm"));
    const SingerInfo singer({QStringLiteral("mix"), QStringLiteral("gui"), QVersionNumber(1, 0)},
                            QStringLiteral("Mix"), {bright, warm});
    SpeakerMixModel::SpeakerMixData fixed;
    fixed.mode = SpeakerMixModel::SingerSourceMode::FixedMix;
    fixed.sources = {{bright}, {warm}};
    fixed.fixedWeights = {0.5};
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.parameters().applyClipSpeakerMix(
        commandContext(), Automation::ClipId(singingClip->id()), singer, bright, fixed));
    ParamEditorView panel;
    const auto detach = qScopeGuard([&] { panel.setDataContext(nullptr); });
    showSpeakerMixPanel(panel, singingClip);
    if (QTest::currentTestFailed())
        return;
    auto *empty = panel.findChild<QWidget *>("speakerMixEmptyState");
    QVERIFY(empty);
    QTRY_VERIFY(empty->isVisible());
    auto *enable = empty->findChild<Button *>();
    QVERIFY(enable);
    QTest::mouseClick(enable, Qt::LeftButton);
    QTRY_VERIFY(!empty->isVisible());
    QCOMPARE(singingClip->speakerMixData().mode, SpeakerMixModel::SingerSourceMode::DynamicMix);
    auto *graphics = panel.graphicsView();
    auto *mix = graphics->speakerMixView();
    QVERIFY(mix);
    QTRY_VERIFY(mix->isVisible() && mix->rect().height() > 200);
    QVERIFY(mix->isEditable());
    QVERIFY(graphics->setViewportScale(1.0, 1.0));
    graphics->setViewportStartTick(0);
    historyManager->reset();
    const auto initial = singingClip->speakerMixData();
    QCOMPARE(initial.dynamicKeyframes.size(), 1);
    const auto anchorId = initial.dynamicKeyframes.first().id;
    QCOMPARE(mix->workingMixData(), initial);
    QCOMPARE(mix->committedMixData(), initial);
    QSignalSpy commits(mix, &SpeakerMixEditorView::speakerMixEdited);

    const auto point = [&](const int tick, const double heightRatio) {
        return speakerMixPoint(*graphics, tick, heightRatio);
    };
    const auto dragTo = [&](const QPoint &position) {
        movePressedMouse(*graphics->viewport(), position);
    };
    const auto addPoint = point(480, 0.5);
    QVERIFY(graphics->viewport()->rect().contains(addPoint));
    QTest::mouseClick(graphics->viewport(), Qt::LeftButton, Qt::NoModifier, addPoint);
    QTest::mouseDClick(graphics->viewport(), Qt::LeftButton, Qt::NoModifier, addPoint);
    QTest::mouseRelease(graphics->viewport(), Qt::LeftButton, Qt::NoModifier, addPoint);
    QCOMPARE(commits.size(), 1);
    const auto afterAdd = singingClip->speakerMixData();
    QCOMPARE(afterAdd.dynamicKeyframes.size(), 2);
    QCOMPARE(afterAdd.dynamicKeyframes.first().id, anchorId);
    const auto added = afterAdd.dynamicKeyframes.last();
    QVERIFY(added.id != anchorId);
    QCOMPARE(added.weights, QVector<double>{0.5});

    const auto beforeNoOp = runtime.documentVersion();
    QTest::mouseClick(graphics->viewport(), Qt::LeftButton, Qt::NoModifier, point(added.tick, 0.9));
    QCOMPARE(runtime.documentVersion(), beforeNoOp);
    QCOMPARE(commits.size(), 1);
    QCOMPARE(mix->workingMixData(), afterAdd);

    const auto movedPoint = point(960, 0.9);
    QTest::mousePress(graphics->viewport(), Qt::LeftButton, Qt::NoModifier, point(added.tick, 0.9));
    dragTo(movedPoint);
    const auto timePreview = mix->workingMixData();
    QVERIFY(timePreview.dynamicKeyframes.last().tick > added.tick);
    QCOMPARE(timePreview.dynamicKeyframes.last().id, added.id);
    QCOMPARE(singingClip->speakerMixData(), afterAdd);
    QTest::mouseRelease(graphics->viewport(), Qt::LeftButton, Qt::NoModifier, movedPoint);
    QCOMPARE(commits.size(), 2);
    const auto afterMove = singingClip->speakerMixData();
    QCOMPARE(afterMove, timePreview);
    QCOMPARE(afterMove.dynamicKeyframes.first().id, anchorId);

    const auto tick = afterMove.dynamicKeyframes.last().tick;
    const auto split = point(tick, 0.5);
    const auto changedWeight = point(tick, 0.75);
    const auto beforeCancel = runtime.documentVersion();
    const auto *undoEntry = historyManager->nextUndoEntry();
    QTest::mousePress(graphics->viewport(), Qt::LeftButton, Qt::NoModifier, split);
    dragTo(changedWeight);
    QVERIFY(mix->workingMixData().dynamicKeyframes.last().weights.first() > 0.5);
    QCOMPARE(singingClip->speakerMixData(), afterMove);
    QTest::keyClick(graphics, Qt::Key_Escape);
    QTest::mouseRelease(graphics->viewport(), Qt::LeftButton, Qt::NoModifier, changedWeight);
    QCOMPARE(runtime.documentVersion(), beforeCancel);
    QCOMPARE(historyManager->nextUndoEntry(), undoEntry);
    QCOMPARE(mix->workingMixData(), afterMove);
    QCOMPARE(commits.size(), 2);

    QTest::mousePress(graphics->viewport(), Qt::LeftButton, Qt::NoModifier, split);
    dragTo(changedWeight);
    const auto weightPreview = mix->workingMixData();
    QTest::mouseRelease(graphics->viewport(), Qt::LeftButton, Qt::NoModifier, changedWeight);
    QCOMPARE(commits.size(), 3);
    QCOMPARE(singingClip->speakerMixData(), weightPreview);
    QCOMPARE(weightPreview.dynamicKeyframes.last().id, added.id);
    QCOMPARE(weightPreview.dynamicKeyframes.first().id, anchorId);
    QVERIFY(weightPreview.dynamicKeyframes.last().weights.first() > 0.5);

    historyManager->undo();
    QCOMPARE(singingClip->speakerMixData(), afterMove);
    QCOMPARE(mix->workingMixData(), afterMove);
    historyManager->undo();
    QCOMPARE(singingClip->speakerMixData(), afterAdd);
    historyManager->undo();
    QCOMPARE(singingClip->speakerMixData(), initial);
    QCOMPARE(mix->workingMixData(), initial);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::dynamicSpeakerMixRangeDeletionAndContextMenu() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    seedDynamicMix(runtime, commandContext(), *singingClip);
    if (QTest::currentTestFailed())
        return;
    ParamEditorView panel;
    const auto detach = qScopeGuard([&] { panel.setDataContext(nullptr); });
    showSpeakerMixPanel(panel, singingClip);
    if (QTest::currentTestFailed())
        return;
    auto *graphics = panel.graphicsView();
    auto *mix = graphics->speakerMixView();
    QVERIFY(graphics->setViewportScale(1.0, 1.0));
    graphics->setViewportStartTick(0);
    QTRY_VERIFY(mix->isVisible() && mix->rect().height() > 200);
    const auto initial = singingClip->speakerMixData();
    const auto anchor = initial.dynamicKeyframes.first();
    historyManager->reset();
    const auto before = runtime.documentVersion();
    QSignalSpy commits(mix, &SpeakerMixEditorView::speakerMixEdited);

    const auto selectFrom = speakerMixPoint(*graphics, 200, 0.9);
    const auto selectTo = speakerMixPoint(*graphics, 1200, 0.1);
    QVERIFY(graphics->viewport()->rect().contains(selectFrom));
    QVERIFY(graphics->viewport()->rect().contains(selectTo));
    QTest::mousePress(graphics->viewport(), Qt::LeftButton, Qt::NoModifier, selectFrom);
    movePressedMouse(*graphics->viewport(), selectTo);
    QTest::mouseRelease(graphics->viewport(), Qt::LeftButton, Qt::NoModifier, selectTo);
    QCOMPARE(singingClip->speakerMixData(), initial);
    QCOMPARE(runtime.documentVersion(), before);
    QTest::keyClick(graphics, Qt::Key_Delete);
    QCOMPARE(commits.size(), 1);
    QCOMPARE(singingClip->speakerMixData().dynamicKeyframes,
             QVector<SpeakerMixModel::SpeakerMixKeyframe>{anchor});
    QCOMPARE(mix->workingMixData(), singingClip->speakerMixData());
    historyManager->undo();
    QCOMPARE(singingClip->speakerMixData(), initial);
    QCOMPARE(mix->workingMixData(), initial);
    QVERIFY(!historyManager->canUndo());

    const auto deleteFromMenu = [&](int tick, bool enabled) {
        const auto position = speakerMixPoint(*graphics, tick, 0.5);
        QVERIFY(graphics->viewport()->rect().contains(position));
        bool opened = false;
        QTimer select;
        select.setSingleShot(true);
        QObject::connect(&select, &QTimer::timeout, graphics, [&] {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(menu);
            const auto close = qScopeGuard([&] { menu->close(); });
            QAction *remove = nullptr;
            for (auto *action : menu->actions()) {
                if (action->text() == SpeakerMixEditorView::tr("Delete"))
                    remove = action;
            }
            QVERIFY(remove);
            QCOMPARE(remove->isEnabled(), enabled);
            opened = true;
            if (enabled)
                QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                                  menu->actionGeometry(remove).center());
        });
        select.start(0);
        QContextMenuEvent event(QContextMenuEvent::Mouse, position,
                                graphics->viewport()->mapToGlobal(position));
        QApplication::sendEvent(graphics->viewport(), &event);
        select.stop();
        QVERIFY(opened);
    };
    deleteFromMenu(480, true);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(commits.size(), 2);
    QCOMPARE(
        singingClip->speakerMixData().dynamicKeyframes,
        (QVector<SpeakerMixModel::SpeakerMixKeyframe>{anchor, initial.dynamicKeyframes.last()}));
    historyManager->undo();
    QCOMPARE(mix->workingMixData(), initial);
    const auto afterUndo = runtime.documentVersion();
    deleteFromMenu(0, false);
    if (QTest::currentTestFailed())
        return;
    QTest::mouseClick(graphics->viewport(), Qt::LeftButton, Qt::NoModifier,
                      speakerMixPoint(*graphics, 0, 0.5));
    QTest::keyClick(graphics, Qt::Key_Delete);
    QCOMPARE(runtime.documentVersion(), afterUndo);
    QCOMPARE(singingClip->speakerMixData(), initial);
    QCOMPARE(commits.size(), 2);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::dynamicSpeakerMixNavigationUsesProjectTime_data() {
    QTest::addColumn<int>("clipStart");
    QTest::newRow("project-start") << 0;
    QTest::newRow("later-clip") << 1920;
}

void ApplicationGuiTests::dynamicSpeakerMixBypassAndStopFollowToolbarInputs() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    seedDynamicMix(runtime, commandContext(), *singingClip);
    if (QTest::currentTestFailed())
        return;
    ParamEditorView panel;
    const auto detach = qScopeGuard([&] { panel.setDataContext(nullptr); });
    showSpeakerMixPanel(panel, singingClip);
    if (QTest::currentTestFailed())
        return;
    auto *toggle = panel.findChild<Button *>("btnDynamicMixBypassToggle");
    auto *stop = panel.findChild<Button *>("btnStopDynamicMix");
    auto *empty = panel.findChild<QWidget *>("speakerMixEmptyState");
    auto *mix = panel.graphicsView()->speakerMixView();
    QVERIFY(toggle && stop && empty && mix);
    QTRY_VERIFY(toggle->isVisible() && stop->isVisible());
    const auto original = singingClip->speakerMixData();
    historyManager->reset();
    QTest::mouseClick(toggle, Qt::LeftButton);
    QVERIFY(toggle->isChecked());
    QVERIFY(singingClip->speakerMixData().dynamicBypassed);
    QCOMPARE(singingClip->speakerMixData().dynamicKeyframes, original.dynamicKeyframes);
    QCOMPARE(mix->workingMixData(), singingClip->speakerMixData());
    QTest::mouseClick(toggle, Qt::LeftButton);
    QVERIFY(!toggle->isChecked());
    QCOMPARE(singingClip->speakerMixData(), original);
    historyManager->undo();
    QVERIFY(toggle->isChecked());
    QVERIFY(singingClip->speakerMixData().dynamicBypassed);
    historyManager->undo();
    QVERIFY(!toggle->isChecked());
    QCOMPARE(singingClip->speakerMixData(), original);
    QVERIFY(!historyManager->canUndo());

    for (bool accept : {false, true}) {
        const auto before = runtime.documentVersion();
        bool answered = false;
        QTimer answer;
        answer.setSingleShot(true);
        QObject::connect(&answer, &QTimer::timeout, &panel, [&] {
            auto *modal = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(modal);
            const auto reject = qScopeGuard([&] { modal->reject(); });
            auto *dialog = qobject_cast<MessageDialog *>(modal);
            QVERIFY(dialog);
            const auto text =
                accept ? ParamEditorView::tr("停止使用动态混合") : ParamEditorView::tr("取消");
            Button *choice = nullptr;
            for (auto *button : dialog->findChildren<Button *>()) {
                if (button->text() == text)
                    choice = button;
            }
            QVERIFY(choice);
            QTest::mouseClick(choice, Qt::LeftButton);
            answered = true;
        });
        answer.start(0);
        QTest::mouseClick(stop, Qt::LeftButton);
        answer.stop();
        QVERIFY(answered);
        if (!accept) {
            QCOMPARE(runtime.documentVersion(), before);
            QCOMPARE(singingClip->speakerMixData(), original);
            QVERIFY(!historyManager->canUndo());
            continue;
        }
        const auto stopped = singingClip->speakerMixData();
        QCOMPARE(stopped.mode, SpeakerMixModel::SingerSourceMode::FixedMix);
        QVERIFY(stopped.dynamicKeyframes.isEmpty());
        QVERIFY(!stopped.dynamicBypassed);
        QCOMPARE(stopped.sources, original.sources);
        QCOMPARE(stopped.fixedWeights, original.dynamicKeyframes.first().weights);
        QTRY_VERIFY(empty->isVisible());
        QVERIFY(!stop->isVisible());
        QVERIFY(!toggle->isVisible());
        QVERIFY(!mix->isEditable());
        historyManager->undo();
        QCOMPARE(singingClip->speakerMixData(), original);
        QCOMPARE(mix->workingMixData(), original);
        QTRY_VERIFY(!empty->isVisible() && stop->isVisible() && toggle->isVisible());
        QVERIFY(mix->isEditable());
        QVERIFY(!historyManager->canUndo());
    }
}

void ApplicationGuiTests::dynamicSpeakerMixNavigationUsesProjectTime() {
    QFETCH(int, clipStart);
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    singingClip->setStart(clipStart);
    auto &runtime = *context->m_coreRuntime;
    seedDynamicMix(runtime, commandContext(), *singingClip);
    if (QTest::currentTestFailed())
        return;
    ParamEditorView panel;
    const auto detach = qScopeGuard([&] { panel.setDataContext(nullptr); });
    showSpeakerMixPanel(panel, singingClip);
    if (QTest::currentTestFailed())
        return;
    auto *previous = panel.findChild<Button *>("btnPrevKeyframe");
    auto *next = panel.findChild<Button *>("btnNextKeyframe");
    QVERIFY(previous && next);
    QTRY_VERIFY(previous->isVisible() && next->isVisible());
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto mix = singingClip->speakerMixData();
    QVERIFY(runtime.playback().setPosition(commandContext(), clipStart));
    QTest::mouseClick(next, Qt::LeftButton);
    QCOMPARE(playbackController->position(), clipStart + 480.0);
    QTRY_VERIFY(panel.graphicsView()->startTick() <= clipStart + 480.0 &&
                panel.graphicsView()->endTick() >= clipStart + 480.0);
    QTest::mouseClick(next, Qt::LeftButton);
    QCOMPARE(playbackController->position(), clipStart + 960.0);
    QTest::mouseClick(next, Qt::LeftButton);
    QCOMPARE(playbackController->position(), clipStart + 960.0);
    QTest::mouseClick(previous, Qt::LeftButton);
    QCOMPARE(playbackController->position(), clipStart + 480.0);
    QTest::mouseClick(previous, Qt::LeftButton);
    QCOMPARE(playbackController->position(), double(clipStart));
    QTest::mouseClick(previous, Qt::LeftButton);
    QCOMPARE(playbackController->position(), double(clipStart));
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(singingClip->speakerMixData(), mix);
    QVERIFY(!historyManager->canUndo());
}
