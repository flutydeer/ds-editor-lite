#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Global/ControllerGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/ClipEditor/PianoRoll/NoteView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsViewHelper.h"

#include <lite/GUI/Controls/ToolTip.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/Tasking/TaskManager.h>

#include <QLabel>
#include <QTextDocument>
#include <QtTest/QTest>

#include <algorithm>

namespace {
    QString plainText(const QLabel *label) {
        if (!label)
            return {};
        QTextDocument document;
        document.setHtml(label->text());
        return document.toPlainText();
    }
}

void ApplicationGuiTests::inferenceErrorBadgesExplainOverlapsAndFollowUndo() {
    createLyricSelection(480);
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    const auto notes = singingClip->notes().toList();
    QCOMPARE(notes.size(), 3);
    const auto *first = notes.at(0);
    const auto *second = notes.at(1);
    const auto *third = notes.at(2);
    const auto *firstItem = sceneNote(first->id());
    QVERIFY(firstItem);
    QVERIFY(singingClip->noteInferenceErrors().isEmpty());
    QVERIFY(!firstItem->hasInferenceError());
    const auto badgeRect =
        view->mapFromScene(firstItem->mapRectToScene(
                               PianoRollGraphicsViewHelper::noteErrorBadgeRect(firstItem->rect())))
            .boundingRect();
    QVERIFY(view->viewport()->rect().contains(badgeRect));
    const auto cleanBadge = view->viewport()->grab(badgeRect).toImage();
    const auto cleanProject = TestSupport::projectSnapshot(*context->m_appModel);
    const auto initialVersion = runtime.documentVersion();
    const auto inferenceSettled = [&] {
        return !singingClip->pieces().isEmpty() && taskManager->tasks().isEmpty() &&
               std::all_of(singingClip->pieces().cbegin(), singingClip->pieces().cend(),
                           [](const InferPiece *piece) {
                               return piece->state == QStringLiteral("Acoustic.Awaiting") ||
                                      piece->state == QStringLiteral("Ready");
                           });
    };
    QVERIFY(runtime.notes().moveNotes(commandContext(), Automation::ClipId(singingClip->id()),
                                      {Automation::NoteId(second->id())}, -240, 0));
    QCOMPARE(runtime.documentVersion().revision, initialVersion.revision + 1);
    const auto *editEntry = historyManager->nextUndoEntry();
    QVERIFY(editEntry);
    QTRY_VERIFY_WITH_TIMEOUT(singingClip->noteInferenceErrors().contains(first->id()) &&
                                 singingClip->noteInferenceErrors().contains(second->id()) &&
                                 inferenceSettled(),
                             15000);
    QCOMPARE(singingClip->noteInferenceErrors().value(first->id()).reason,
             SliceExclusionReason::Overlapped);
    QVERIFY(!singingClip->noteInferenceErrors().contains(third->id()));
    QVERIFY(firstItem->hasInferenceError());
    QVERIFY(sceneNote(second->id())->hasInferenceError());
    QVERIFY(!sceneNote(third->id())->hasInferenceError());
    QVERIFY(view->viewport()->grab(badgeRect).toImage() != cleanBadge);
    QCOMPARE(historyManager->nextUndoEntry(), editEntry);
    const auto overlappingVersion = runtime.documentVersion();
    const auto overlappingProject = TestSupport::projectSnapshot(*context->m_appModel);
    const auto *undoEntry = historyManager->nextUndoEntry();
    const auto selected = appStatus->selectedNotes.get();
    const auto visibleErrorToolTip = [&]() -> ToolTip * {
        for (auto *tip : view->findChildren<ToolTip *>())
            if (tip->isVisible() && plainText(tip->findChild<QLabel *>("toolTipTitle")) ==
                                        QStringLiteral("Overlapping note"))
                return tip;
        return nullptr;
    };
    const auto badgeCenter = badgeRect.center();
    QTest::mouseMove(view->viewport(), pointFor(1800, 66));
    QTest::mouseMove(view->viewport(), badgeCenter);
    QTRY_VERIFY(visibleErrorToolTip());
    auto *tip = visibleErrorToolTip();
    QCOMPARE(plainText(tip->findChild<QLabel *>("toolTipMessage")),
             QStringLiteral("This note overlaps another note and is ignored"));
    QTest::mouseMove(view->viewport(), pointFor(1800, 66));
    QTRY_VERIFY(!visibleErrorToolTip());
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, badgeCenter);
    QTRY_VERIFY(visibleErrorToolTip());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(appStatus->selectedNotes.get(), selected);
    QCOMPARE(runtime.documentVersion(), overlappingVersion);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), overlappingProject);
    QCOMPARE(historyManager->nextUndoEntry(), undoEntry);

    QVERIFY(runtime.history().undo(commandContext()));
    QTRY_VERIFY_WITH_TIMEOUT(singingClip->noteInferenceErrors().isEmpty() && inferenceSettled(),
                             15000);
    QVERIFY(!firstItem->hasInferenceError());
    QVERIFY(!sceneNote(second->id())->hasInferenceError());
    QTRY_VERIFY(!visibleErrorToolTip());
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), cleanProject);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(runtime.history().redo(commandContext()));
    QTRY_VERIFY_WITH_TIMEOUT(
        singingClip->noteInferenceErrors().contains(first->id()) && inferenceSettled(), 15000);
    QVERIFY(firstItem->hasInferenceError());
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), overlappingProject);
}
