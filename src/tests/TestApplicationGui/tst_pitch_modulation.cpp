#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsScene.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include "UI/Views/ClipEditor/PianoRoll/PitchEditorView.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/Tasking/TaskManager.h>

#include <QApplication>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QtTest/QTest>

#include <algorithm>

void ApplicationGuiTests::pitchModulationUsesTheInferredNoteBaselineAndCanBeUndone() {
    createLyricSelection();
    if (QTest::currentTestFailed())
        return;
    for (const auto *note : singingClip->notes())
        QCOMPARE(note->keyIndex(), 60);

    auto &runtime = *context->m_coreRuntime;
    Automation::CurveDraftDto draft;
    draft.type = Automation::CurveDraftDto::Type::Draw;
    draft.localStart = 240;
    draft.step = 5;
    draft.values = QList<int>((1200 - draft.localStart) / draft.step, 6200);
    QVERIFY(runtime.parameters().replaceParameter(commandContext(),
                                                  Automation::ClipId(singingClip->id()),
                                                  ParamInfo::Pitch, Param::Edited, {draft}));
    const auto inferenceSettled = [&] {
        return std::all_of(singingClip->pieces().cbegin(), singingClip->pieces().cend(),
                           [](const InferPiece *piece) {
                               return piece->state == QStringLiteral("Acoustic.Awaiting") ||
                                      piece->state == QStringLiteral("Ready");
                           }) &&
               taskManager->tasks().isEmpty();
    };
    QCoreApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(inferenceSettled(), 15000);

    auto *parameter = singingClip->params.getParamByName(ParamInfo::Pitch);
    QVERIFY(parameter);
    QVERIFY(!parameter->curves(Param::Original).isEmpty());
    view->setEditMode(ClipEditorGlobal::ModulatePitch);
    view->setViewportStartTick(0);
    PitchEditorView *editor = nullptr;
    for (auto *item : scene->items()) {
        if (auto *pitch = dynamic_cast<PitchEditorView *>(item))
            editor = pitch;
    }
    QVERIFY(editor);
    QVERIFY(!editor->transparentMouseEvents());
    QCoreApplication::processEvents();
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto snapshot = [&] {
        QList<DrawCurve> curves;
        for (const auto *curve : parameter->curves(Param::Edited))
            curves.append(*static_cast<const DrawCurve *>(curve));
        return curves;
    };
    const auto original = snapshot();
    const auto valueAt = [](const auto &curves, const int tick) {
        for (const auto *source : curves) {
            const auto *curve = dynamic_cast<const DrawCurve *>(source);
            if (curve && curve->localStart() <= tick && tick < curve->localEndTick())
                return curve->values().at((tick - curve->localStart()) / curve->step);
        }
        return -1;
    };
    const auto pointAt = [&](const int tick) {
        return view->mapFromScene(QPointF(view->tickToSceneX(tick), editor->sceneYForValue(6000)));
    };
    const auto moveWithLeftButton = [&](const QPoint &position) {
        QMouseEvent event(QEvent::MouseMove, QPointF(position),
                          QPointF(view->viewport()->mapToGlobal(position)), Qt::NoButton,
                          Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(view->viewport(), &event);
    };
    const auto start = pointAt(480);
    const auto end = pointAt(960);
    const auto press = pointAt(720);
    const auto release = view->mapFromScene(view->mapToScene(press) + QPointF(0, 100));
    for (const auto &point : {start, end, press, release})
        QVERIFY(view->viewport()->rect().contains(point));
    const auto cancelOnFailure = qScopeGuard([&] {
        if (QTest::currentTestFailed()) {
            QTest::keyClick(view.get(), Qt::Key_Escape);
            QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, release);
        }
    });
    const auto selectRange = [&] {
        QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, start);
        moveWithLeftButton(end);
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, end);
        QVERIFY(!editSessionManager->hasActiveTransaction());
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(snapshot(), original);
    };
    QSignalSpy committed(editor, &CommonParamEditorView::editCommitted);
    QSignalSpy discarded(editor, &CommonParamEditorView::editDiscarded);
    QObject commitObserver;
    auto userCommitVersion = before;
    // The production connection commits before editCommitted releases pending inference results.
    connect(editor, &CommonParamEditorView::editCompleted, &commitObserver,
            [&] { userCommitVersion = runtime.documentVersion(); });
    selectRange();
    if (QTest::currentTestFailed())
        return;
    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, press);
    moveWithLeftButton(release);
    QVERIFY(editSessionManager->hasActiveTransaction());
    // A zero modulation factor follows the actual C4 notes, not the edited D4 curve.
    QCOMPARE(valueAt(editor->editedCurves(), 720), 6000);
    QCOMPARE(valueAt(editor->editedCurves(), 240), 6200);
    QCOMPARE(valueAt(editor->editedCurves(), 1195), 6200);
    QCOMPARE(snapshot(), original);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());

    QTest::keyClick(view.get(), Qt::Key_Escape);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, release);
    QCOMPARE(discarded.count(), 1);
    QCOMPARE(committed.count(), 0);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(valueAt(editor->editedCurves(), 720), 6200);
    QCOMPARE(snapshot(), original);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());

    selectRange();
    if (QTest::currentTestFailed())
        return;
    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, press);
    moveWithLeftButton(release);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, release);
    QCOMPARE(committed.count(), 1);
    QCOMPARE(userCommitVersion.documentId, before.documentId);
    QCOMPARE(userCommitVersion.revision, before.revision + 1);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QTRY_VERIFY_WITH_TIMEOUT(inferenceSettled(), 15000);
    QCOMPARE(valueAt(parameter->curves(Param::Edited), 720), 6000);
    QCOMPARE(valueAt(parameter->curves(Param::Edited), 240), 6200);
    QCOMPARE(valueAt(parameter->curves(Param::Edited), 1195), 6200);
    QVERIFY(historyManager->canUndo());

    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(snapshot(), original);
    QCOMPARE(valueAt(editor->editedCurves(), 720), 6200);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(historyManager->canRedo());
}
