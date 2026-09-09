#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Model/Utils/ParamUtils.h"
#include "UI/Views/ClipEditor/ParamEditor/ParamEditorView.h"
#include "UI/Views/ClipEditor/ParamEditor/ParamEditorGraphicsView.h"
#include "UI/Views/ClipEditor/ParamEditor/SpeakerMixEditorView.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QApplication>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QtTest/QTest>

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
    panel.setDataContext(singingClip);
    const auto detach = qScopeGuard([&] { panel.setDataContext(nullptr); });
    panel.resize(900, 360);
    panel.show();
    panel.activateWindow();
    auto *foreground = panel.findChild<ComboBox *>("cbForegroundParam");
    QVERIFY(foreground);
    const auto mixIndex = foreground->findText(paramUtils->nameFromType(ParamInfo::SpeakerMix));
    QVERIFY(mixIndex >= 0);
    QTest::mouseClick(foreground, Qt::LeftButton);
    QTRY_VERIFY(foreground->view()->isVisible());
    const auto item = foreground->model()->index(mixIndex, 0);
    foreground->view()->scrollTo(item);
    QTest::mouseClick(foreground->view()->viewport(), Qt::LeftButton, Qt::NoModifier,
                      foreground->view()->visualRect(item).center());
    QCOMPARE(panel.viewState().foreground, ParamInfo::SpeakerMix);
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
        const auto y = mix->mapToScene(QPointF(0, mix->rect().height() * heightRatio)).y();
        return graphics->mapFromScene(QPointF(graphics->tickToSceneX(tick), y));
    };
    const auto dragTo = [&](const QPoint &position) {
        QMouseEvent move(QEvent::MouseMove, QPointF(position),
                         QPointF(graphics->viewport()->mapToGlobal(position)), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(graphics->viewport(), &move);
    };
    const auto addPoint = point(480, 0.5);
    QVERIFY(graphics->viewport()->rect().contains(addPoint));
    QTest::mouseDClick(graphics->viewport(), Qt::LeftButton, Qt::NoModifier, addPoint);
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
