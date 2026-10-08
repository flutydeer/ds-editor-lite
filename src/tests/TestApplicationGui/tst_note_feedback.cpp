#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Global/ControllerGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/ClipEditor/PianoRoll/NoteView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsViewHelper.h"
#include "UI/Views/Common/TimelineView.h"

#include <lite/GUI/Controls/ToolTip.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
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

void ApplicationGuiTests::inferenceErrorBadgesExplainOverlapsAndFollowUndo_data() {
    QTest::addColumn<bool>("allNotesOverlap");
    QTest::newRow("partially-overlapping-phrase") << false;
    QTest::newRow("entire-phrase-excluded") << true;
}

void ApplicationGuiTests::inferenceErrorBadgesExplainOverlapsAndFollowUndo() {
    QFETCH(bool, allNotesOverlap);
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
    TimelineView timeline;
    timeline.resize(900, 40);
    const auto timelineStart = singingClip->start();
    timeline.setTimeRange(timelineStart, timelineStart + 3840);
    timeline.setDataContext(singingClip);
    const auto indicatorColor = [&] {
        const auto image = timeline.grab().toImage();
        return image.pixelColor(qRound(720.0 * image.width() / 3840), image.height() - 1);
    };
    const auto cleanIndicator = indicatorColor();
    const auto failedIndicator = timeline.property("pieceFailedColor").value<QColor>();
    QVERIFY(cleanIndicator != failedIndicator);
    const auto cleanProject = TestSupport::projectSnapshot(*context->m_appModel);
    const auto initialVersion = runtime.documentVersion();
    const auto inferenceSettled = [&] {
        if (!taskManager->tasks().isEmpty())
            return false;
        if (singingClip->pieces().isEmpty())
            return allNotesOverlap && singingClip->noteInferenceErrors().size() == notes.size();
        return std::all_of(singingClip->pieces().cbegin(), singingClip->pieces().cend(),
                           [](const InferPiece *piece) {
                               return piece->state == QStringLiteral("Acoustic.Awaiting") ||
                                      piece->state == QStringLiteral("Ready");
                           });
    };
    QList<Automation::NoteId> moved{Automation::NoteId(second->id())};
    if (allNotesOverlap)
        moved.append(Automation::NoteId(third->id()));
    QVERIFY(runtime.notes().moveNotes(commandContext(), Automation::ClipId(singingClip->id()),
                                      moved, allNotesOverlap ? -600 : -240, 0));
    QCOMPARE(runtime.documentVersion().revision, initialVersion.revision + 1);
    const auto *editEntry = historyManager->nextUndoEntry();
    QVERIFY(editEntry);
    QTRY_VERIFY_WITH_TIMEOUT(singingClip->noteInferenceErrors().contains(first->id()) &&
                                 singingClip->noteInferenceErrors().contains(second->id()) &&
                                 inferenceSettled(),
                             15000);
    QCOMPARE(singingClip->noteInferenceErrors().value(first->id()).reason,
             SliceExclusionReason::Overlapped);
    QCOMPARE(singingClip->noteInferenceErrors().contains(third->id()), allNotesOverlap);
    QVERIFY(firstItem->hasInferenceError());
    QVERIFY(sceneNote(second->id())->hasInferenceError());
    QCOMPARE(sceneNote(third->id())->hasInferenceError(), allNotesOverlap);
    QVERIFY(view->viewport()->grab(badgeRect).toImage() != cleanBadge);
    if (allNotesOverlap) {
        QVERIFY(singingClip->pieces().isEmpty());
        QVERIFY(!singingClip->skippedPhraseRanges().isEmpty());
        QTRY_COMPARE(indicatorColor(), failedIndicator);
    }
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
    if (allNotesOverlap) {
        QVERIFY(singingClip->skippedPhraseRanges().isEmpty());
        QTRY_COMPARE(indicatorColor(), cleanIndicator);
    }
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), cleanProject);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(runtime.history().redo(commandContext()));
    QTRY_VERIFY_WITH_TIMEOUT(
        singingClip->noteInferenceErrors().contains(first->id()) && inferenceSettled(), 15000);
    QVERIFY(firstItem->hasInferenceError());
    if (allNotesOverlap) {
        QVERIFY(singingClip->pieces().isEmpty());
        QVERIFY(!singingClip->skippedPhraseRanges().isEmpty());
        QTRY_COMPARE(indicatorColor(), failedIndicator);
    }
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), overlappingProject);
}

void ApplicationGuiTests::timelineContextSwitchKeepsCurrentInferenceFeedback() {
    class ObservedTimeline final : public TimelineView {
    public:
        int paintCount = 0;

    protected:
        void paintEvent(QPaintEvent *event) override {
            TimelineView::paintEvent(event);
            ++paintCount;
        }
    };

    const auto createClip = [&] {
        auto *note = new Note;
        note->setLocalStart(480);
        note->setLength(480);
        note->setLyric(QStringLiteral("a"));
        PhonemeName vowel;
        vowel.language = QStringLiteral("eng");
        vowel.name = QStringLiteral("a");
        vowel.isOnset = true;
        note->setPhonemeNameSeq(Note::Original, {vowel});
        auto clip = std::make_unique<SingingClip>(QList{note});
        clip->setLength(1440);
        clip->reSegment(context->m_appModel->timeline());
        return clip;
    };
    auto previous = createClip();
    auto current = createClip();
    QCOMPARE(previous->pieces().size(), 1);
    QCOMPARE(current->pieces().size(), 1);
    const auto document = TestSupport::projectSnapshot(*context->m_appModel);
    const auto version = context->m_coreRuntime->documentVersion();
    const auto *undo = historyManager->nextUndoEntry();
    auto *piece = current->pieces().first();
    piece->acousticInferStatus = Running;
    ObservedTimeline timeline;
    timeline.resize(900, 40);
    timeline.setTimeRange(0, 1440);
    timeline.setDataContext(previous.get());
    timeline.setDataContext(current.get());
    timeline.show();
    QTRY_VERIFY(timeline.paintCount > 0);

    previous->removeAllPieces();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
    QCoreApplication::processEvents();
    const auto beforeCompletion = timeline.paintCount;
    piece->acousticInferStatus = Success;
    QTRY_VERIFY(timeline.paintCount > beforeCompletion);
    const auto success = timeline.property("pieceSuccessColor").value<QColor>();
    QTRY_COMPARE(timeline.grab().toImage().pixelColor(timeline.width() / 2, timeline.height() - 1),
                 success);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
    QCoreApplication::processEvents();
    const auto settled = timeline.paintCount;
    QTest::qWait(50);
    QCOMPARE(timeline.paintCount, settled);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), document);
    QCOMPARE(context->m_coreRuntime->documentVersion(), version);
    QCOMPARE(historyManager->nextUndoEntry(), undo);
}
