#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/ClipController.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollCoord.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollContextMenuController.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollView.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QImage>
#include <QMenu>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QTimer>
#include <QWindow>
#include <QtTest/QTest>

namespace {
    struct AnchorPianoFixture {
        ~AnchorPianoFixture() {
            if (canvas) {
                QTest::keyClick(canvas, Qt::Key_Escape);
                QTest::mouseRelease(canvas->viewport(), Qt::LeftButton);
            }
            piano.setDataContext(nullptr);
        }

        void initialize(SingingClip *clip) {
            piano.setDataContext(clip);
            piano.resize(1000, 600);
            piano.show();
            piano.activateWindow();
            QTRY_VERIFY(piano.isActiveWindow());
            canvas = piano.findChild<PianoRollGraphicsView *>();
            QVERIFY(canvas);
            canvas->setAnimationEnabled(false);
            QVERIFY(piano.setViewScale(1.0, 1.0));
            QVERIFY(piano.centerAt(clip->start() + 1920, 62));
            canvas->setViewportStartTick(clip->start());
            piano.onEditModeChanged(ClipEditorGlobal::EditPitchAnchor);
            canvas->setFocus();
            QTRY_VERIFY(canvas->viewport()->width() > 800);
        }

        QPoint pointAt(int localTick, int key) const {
            return canvas->mapFromScene(
                QPointF(canvas->tickToSceneX(localTick),
                        PianoRollCoord::keyIndexToCenterY(key, ClipEditorGlobal::noteHeight *
                                                                   canvas->scaleY())));
        }

        void moveTo(const QPoint &position, Qt::MouseButtons buttons = Qt::NoButton) {
            QMouseEvent event(QEvent::MouseMove, QPointF(position),
                              QPointF(canvas->viewport()->mapToGlobal(position)), Qt::NoButton,
                              buttons, Qt::NoModifier);
            QApplication::sendEvent(canvas->viewport(), &event);
        }

        void enterAt(const QPoint &position) {
            const auto global = canvas->viewport()->mapToGlobal(position);
            QTest::mouseMove(piano.windowHandle(), piano.mapFromGlobal(global));
            moveTo(position);
        }

        void chooseMenu(const QPoint &position, const QString &label) {
            bool activated = false;
            QTimer choose;
            choose.setSingleShot(true);
            QObject::connect(&choose, &QTimer::timeout, &piano, [&] {
                auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                QVERIFY(menu);
                const auto close = qScopeGuard([&] { menu->close(); });
                for (auto *action : menu->actions()) {
                    if (action->text() == label) {
                        QVERIFY(action->isEnabled());
                        QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                                          menu->actionGeometry(action).center());
                        activated = true;
                        return;
                    }
                }
                QFAIL(qPrintable(label));
            });
            enterAt(position);
            QContextMenuEvent event(QContextMenuEvent::Mouse, position,
                                    canvas->viewport()->mapToGlobal(position));
            choose.start(0);
            QApplication::sendEvent(canvas->viewport(), &event);
            choose.stop();
            QVERIFY(activated);
        }

        QImage image() const {
            return canvas->viewport()->grab().toImage();
        }

        PianoRollView piano;
        PianoRollGraphicsView *canvas = nullptr;
    };

    const AnchorCurve *anchorCurve(const SingingClip &clip, qsizetype index = 0) {
        return dynamic_cast<const AnchorCurve *>(
            clip.params.getParamByName(ParamInfo::Pitch)->curves(Param::Edited).value(index));
    }
}

void ApplicationGuiTests::pitchAnchorCreationPreviewsBeforeCommitting() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.project().patchClipProperties(
        commandContext(), {.id = Automation::ClipId(singingClip->id()), .start = 2400}));
    view->hide();
    AnchorPianoFixture fixture;
    fixture.initialize(singingClip);
    if (QTest::currentTestFailed())
        return;
    const auto first = fixture.pointAt(480, 60);
    const auto last = fixture.pointAt(960, 62);
    QVERIFY(fixture.canvas->viewport()->rect().contains(first));
    QVERIFY(fixture.canvas->viewport()->rect().contains(last));
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto createFirst = [&] {
        fixture.enterAt(first);
        QTest::mouseDClick(fixture.canvas->viewport(), Qt::LeftButton, Qt::NoModifier, first);
        QTest::mouseRelease(fixture.canvas->viewport(), Qt::LeftButton, Qt::NoModifier, first);
    };
    createFirst();
    QVERIFY(editSessionManager->hasActiveTransaction());
    QVERIFY(!anchorCurve(*singingClip));
    const auto singleAnchor = fixture.image();
    fixture.moveTo(last);
    const auto preview = fixture.image();
    QVERIFY(!preview.isNull() && preview != singleAnchor);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    QTest::keyClick(fixture.canvas, Qt::Key_Escape);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QVERIFY(!anchorCurve(*singingClip));
    QVERIFY(fixture.image() != preview);
    QCOMPARE(runtime.documentVersion(), before);
    createFirst();
    fixture.moveTo(last);
    QTest::mouseClick(fixture.canvas->viewport(), Qt::LeftButton, Qt::NoModifier, last);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    const auto *curve = anchorCurve(*singingClip);
    QVERIFY(curve);
    const auto nodes = curve->nodes().toList();
    QCOMPARE(nodes.size(), 2);
    QVERIFY(qAbs(nodes.first()->pos() - 480) <= 4);
    QVERIFY(qAbs(nodes.last()->pos() - 960) <= 4);
    QCOMPARE(nodes.first()->value(), 6000);
    QCOMPARE(nodes.last()->value(), 6200);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QVERIFY(runtime.history().undo(commandContext()));
    QVERIFY(!anchorCurve(*singingClip));
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::pitchAnchorRangeEditsUseTheViewAndMenu() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    Automation::CurveDraftDto draft;
    draft.type = Automation::CurveDraftDto::Type::Anchor;
    draft.nodes = {
        {240, 6000, AnchorNode::Hermite},
        {480, 6200, AnchorNode::Hermite},
        {720, 6000, AnchorNode::Hermite},
        {960, 6200, AnchorNode::None   }
    };
    QVERIFY(runtime.parameters().replaceParameter(commandContext(),
                                                  Automation::ClipId(singingClip->id()),
                                                  ParamInfo::Pitch, Param::Edited, {draft}));
    view->hide();
    AnchorPianoFixture fixture;
    fixture.initialize(singingClip);
    if (QTest::currentTestFailed())
        return;
    const auto selectRange = [&] {
        QTest::keyClick(fixture.canvas, Qt::Key_Escape);
        const auto start = fixture.pointAt(180, 64);
        const auto end = fixture.pointAt(780, 58);
        fixture.enterAt(start);
        const auto idle = fixture.image();
        QTest::mousePress(fixture.canvas->viewport(), Qt::LeftButton, Qt::NoModifier, start);
        fixture.moveTo(end, Qt::LeftButton);
        QVERIFY(fixture.image() != idle);
        QTest::mouseRelease(fixture.canvas->viewport(), Qt::LeftButton, Qt::NoModifier, end);
        QVERIFY(!editSessionManager->hasActiveTransaction());
    };
    selectRange();
    if (QTest::currentTestFailed())
        return;
    fixture.chooseMenu(fixture.pointAt(480, 62), PianoRollContextMenuController::tr("Linear"));
    if (QTest::currentTestFailed())
        return;
    auto nodes = anchorCurve(*singingClip)->nodes().toList();
    QCOMPARE(nodes.size(), 4);
    for (int index = 0; index < 3; ++index)
        QCOMPARE(nodes[index]->interpMode(), AnchorNode::Linear);
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto press = fixture.pointAt(480, 62);
    const auto release = fixture.pointAt(600, 63);
    const auto drag = [&] {
        selectRange();
        if (QTest::currentTestFailed())
            return;
        QTest::mousePress(fixture.canvas->viewport(), Qt::LeftButton, Qt::NoModifier, press);
        fixture.moveTo(release, Qt::LeftButton);
        QVERIFY(editSessionManager->hasActiveTransaction());
        const auto current = anchorCurve(*singingClip)->nodes().toList();
        QCOMPARE(current[0]->pos(), 240);
        QCOMPARE(current[1]->value(), 6200);
        QVERIFY(!fixture.image().isNull());
    };
    drag();
    if (QTest::currentTestFailed())
        return;
    QTest::keyClick(fixture.canvas, Qt::Key_Escape);
    QTest::mouseRelease(fixture.canvas->viewport(), Qt::LeftButton, Qt::NoModifier, release);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    drag();
    if (QTest::currentTestFailed())
        return;
    QTest::mouseRelease(fixture.canvas->viewport(), Qt::LeftButton, Qt::NoModifier, release);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    nodes = anchorCurve(*singingClip)->nodes().toList();
    for (int index = 0; index < 3; ++index) {
        QVERIFY(qAbs(nodes[index]->pos() - (draft.nodes[index].position + 120)) <= 4);
        QCOMPARE(nodes[index]->value(), draft.nodes[index].value + 100);
    }
    QCOMPARE(nodes.last()->pos(), 960);
    QCOMPARE(nodes.last()->value(), 6200);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QVERIFY(runtime.history().undo(commandContext()));
    QVERIFY(!historyManager->canUndo());
    selectRange();
    if (QTest::currentTestFailed())
        return;
    fixture.chooseMenu(fixture.pointAt(480, 62), PianoRollContextMenuController::tr("&Delete"));
    if (QTest::currentTestFailed())
        return;
    QVERIFY(!anchorCurve(*singingClip));
    QVERIFY(runtime.history().undo(commandContext()));
    QVERIFY(anchorCurve(*singingClip));
    QCOMPARE(anchorCurve(*singingClip)->nodes().count(), 4);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::pitchAnchorMergePreviewCommitsAndUndoes() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    Automation::CurveDraftDto left;
    left.type = Automation::CurveDraftDto::Type::Anchor;
    left.nodes = {
        {240, 6000, AnchorNode::Linear},
        {360, 6100, AnchorNode::Linear},
        {480, 6100, AnchorNode::None  }
    };
    auto right = left;
    right.nodes = {
        {960,  6200, AnchorNode::Hermite},
        {1440, 6400, AnchorNode::None   }
    };
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.parameters().replaceParameter(commandContext(),
                                                  Automation::ClipId(singingClip->id()),
                                                  ParamInfo::Pitch, Param::Edited, {left, right}));
    view->hide();
    AnchorPianoFixture fixture;
    fixture.initialize(singingClip);
    if (QTest::currentTestFailed())
        return;
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto from = fixture.pointAt(480, 61);
    const auto to = fixture.pointAt(960, 62);
    fixture.enterAt(from);
    QTest::mouseClick(fixture.canvas->viewport(), Qt::LeftButton, Qt::NoModifier, from);
    const auto selected = fixture.image();
    fixture.moveTo(to);
    QVERIFY(fixture.image() != selected);
    QVERIFY(anchorCurve(*singingClip, 1));
    QCOMPARE(runtime.documentVersion(), before);
    QTest::mouseClick(fixture.canvas->viewport(), Qt::LeftButton, Qt::NoModifier, to);
    QVERIFY(anchorCurve(*singingClip));
    QVERIFY(!anchorCurve(*singingClip, 1));
    const auto nodes = anchorCurve(*singingClip)->nodes().toList();
    QCOMPARE(nodes.size(), 5);
    QCOMPARE(nodes.first()->pos(), 240);
    QCOMPARE(nodes.last()->pos(), 1440);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(anchorCurve(*singingClip)->nodes().count(), 3);
    QVERIFY(anchorCurve(*singingClip, 1));
    QCOMPARE(anchorCurve(*singingClip, 1)->nodes().count(), 2);
    QVERIFY(!historyManager->canUndo());

    const auto sourceNodes = anchorCurve(*singingClip)->nodes().toList();
    const auto movedId = sourceNodes.at(1)->id();
    const auto sourceModel = context->m_appModel->serialize();
    const auto sourceVersion = runtime.documentVersion();
    const auto dragFrom = fixture.pointAt(360, 61);
    const auto dragTo = fixture.pointAt(1200, 63);
    const auto drag = [&] {
        QTest::keyClick(fixture.canvas, Qt::Key_Escape);
        fixture.enterAt(dragFrom);
        const auto idle = fixture.image();
        QTest::mousePress(fixture.canvas->viewport(), Qt::LeftButton, Qt::NoModifier, dragFrom);
        fixture.moveTo(dragTo, Qt::LeftButton);
        QVERIFY(editSessionManager->hasActiveTransaction());
        QVERIFY(fixture.image() != idle);
        QCOMPARE(context->m_appModel->serialize(), sourceModel);
        QCOMPARE(runtime.documentVersion(), sourceVersion);
    };
    drag();
    if (QTest::currentTestFailed())
        return;
    QTest::keyClick(fixture.canvas, Qt::Key_Escape);
    QTest::mouseRelease(fixture.canvas->viewport(), Qt::LeftButton, Qt::NoModifier, dragTo);
    QCOMPARE(context->m_appModel->serialize(), sourceModel);
    QVERIFY(!historyManager->canUndo());
    drag();
    if (QTest::currentTestFailed())
        return;
    QTest::mouseRelease(fixture.canvas->viewport(), Qt::LeftButton, Qt::NoModifier, dragTo);
    QCOMPARE(anchorCurve(*singingClip)->nodes().count(), 2);
    const auto targetNodes = anchorCurve(*singingClip, 1)->nodes().toList();
    QCOMPARE(targetNodes.size(), 3);
    QCOMPARE(targetNodes.at(1)->id(), movedId);
    QVERIFY(qAbs(targetNodes.at(1)->pos() - 1200) <= 4);
    QCOMPARE(targetNodes.at(1)->value(), 6300);
    QCOMPARE(runtime.documentVersion().revision, sourceVersion.revision + 1);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(context->m_appModel->serialize(), sourceModel);
    QVERIFY(!historyManager->canUndo());
}
