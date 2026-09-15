#include "tst_application_gui.h"
#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/TrackController.h"
#include "Controller/PlaybackController.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "TestSupport/WaveFixture.h"
#include "UI/Views/TrackEditor/GraphicsItem/AbstractClipView.h"
#include "UI/Views/TrackEditor/TrackEditorView.h"
#include "UI/Views/TrackEditor/TracksGraphicsView.h"
#include "UI/Views/Common/TimelineView.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/Tasking/TaskManager.h>

#include <QApplication>
#include <QCursor>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QtTest/QTest>

#include <tuple>

void ApplicationGuiTests::trackClipDragContinuesDuringEdgeScrollingAndStopsOnFinish() {
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    TrackEditorView editor;
    const auto clearParent = qScopeGuard([] { trackController->setParentWidget(nullptr); });
    Automation::TrackDraftDto trackDraft;
    Automation::ClipDraftDto clipDraft;
    clipDraft.type = Automation::ClipDraftDto::Type::Singing;
    clipDraft.properties.start = 480;
    clipDraft.properties.length = 480;
    clipDraft.properties.clipLen = 480;
    trackDraft.clips.append(clipDraft);
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, trackDraft));
    const auto *clip = *context->m_appModel->tracks().first()->clips().begin();
    const auto clipId = clip->id();
    auto *canvas = editor.findChild<TracksGraphicsView *>();
    QVERIFY(canvas);
    editor.resize(1000, 500);
    editor.show();
    editor.activateWindow();
    QTRY_VERIFY(editor.isActiveWindow());
    QVERIFY(editor.windowHandle());
    canvas->setAnimationEnabled(false);
    QVERIFY(canvas->setViewportScale(3.0, 1.0));
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto previousCursor = QCursor::pos();
    QPoint edge;
    bool pressed = false;
    const auto releaseInput = qScopeGuard([&] {
        if (pressed) {
            canvas->discardAction();
            QTest::mouseRelease(editor.windowHandle(), Qt::LeftButton, Qt::NoModifier,
                                canvas->viewport()->mapTo(&editor, edge));
        }
        QCursor::setPos(previousCursor);
    });
    for (const bool cancel : {true, false}) {
        canvas->setViewportStartTick(0);
        QCoreApplication::processEvents();
        auto *item = editor.findClipItemById(clipId);
        QVERIFY(item);
        const auto press = canvas->mapFromScene(item->sceneBoundingRect().center());
        edge = QPoint(canvas->viewport()->rect().right() - 1, press.y());
        QVERIFY(canvas->viewport()->rect().contains(press));
        const auto windowPoint = [&](const QPoint &point) {
            return canvas->viewport()->mapTo(&editor, point);
        };
        QCursor::setPos(canvas->viewport()->mapToGlobal(press));
        QTest::mouseMove(editor.windowHandle(), windowPoint(press));
        QTest::mousePress(editor.windowHandle(), Qt::LeftButton, Qt::NoModifier,
                          windowPoint(press));
        pressed = true;
        QVERIFY(editSessionManager->hasActiveTransaction());
        QCursor::setPos(canvas->viewport()->mapToGlobal(edge));
        QTest::mouseMove(editor.windowHandle(), windowPoint(edge));
        const auto firstPreview = item->start();
        const auto firstVisibleTick = canvas->startTick();
        // The timer must advance both the viewport and the preview without another move event.
        QTRY_VERIFY(canvas->startTick() > firstVisibleTick && item->start() > firstPreview);
        const auto lastPreview = item->start();
        QCOMPARE(item->clipLen(), clipDraft.properties.clipLen);
        QCOMPARE(clip->start(), clipDraft.properties.start);
        QCOMPARE(runtime.documentVersion(), before);
        QVERIFY(!historyManager->canUndo());
        if (cancel)
            QTest::keyClick(canvas, Qt::Key_Escape);
        QTest::mouseRelease(editor.windowHandle(), Qt::LeftButton, Qt::NoModifier,
                            windowPoint(edge));
        pressed = false;
        QVERIFY(!editSessionManager->hasActiveTransaction());
        if (cancel)
            QCOMPARE(clip->start(), clipDraft.properties.start);
        else
            QVERIFY(clip->start() >= lastPreview);
        auto *finishedItem = editor.findClipItemById(clipId);
        QVERIFY(finishedItem);
        QCOMPARE(finishedItem->start(), clip->start());
        QCOMPARE(finishedItem->clipLen(), clipDraft.properties.clipLen);
        const auto stoppedAt = canvas->startTick();
        QTest::qWait(100);
        QCOMPARE(canvas->startTick(), stoppedAt);
        QCOMPARE(runtime.documentVersion().revision, before.revision + (cancel ? 0 : 1));
    }
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(clip->start(), clipDraft.properties.start);
    QVERIFY(editor.findClipItemById(clipId));
    QCOMPARE(editor.findClipItemById(clipId)->start(), clipDraft.properties.start);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::trackClipDragCommitsOrCancels_data() {
    QTest::addColumn<bool>("audio");
    QTest::addColumn<int>("edge");
    QTest::addColumn<bool>("cancel");
    QTest::newRow("singing-move") << false << 0 << false;
    QTest::newRow("singing-cancel-move") << false << 0 << true;
    QTest::newRow("singing-trim-left") << false << -1 << false;
    QTest::newRow("singing-extend-right") << false << 1 << false;
    QTest::newRow("audio-move") << true << 0 << false;
    QTest::newRow("audio-trim-left") << true << -1 << false;
    QTest::newRow("audio-extend-right") << true << 1 << false;
    QTest::newRow("audio-cancel-trim") << true << -1 << true;
}

void ApplicationGuiTests::trackClipDragCommitsOrCancels() {
    QFETCH(bool, audio);
    QFETCH(int, edge);
    QFETCH(bool, cancel);
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto clearDocument = qScopeGuard([&] {
        trackController->setParentWidget(nullptr);
        runtime.documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false));
        const auto finished = QTest::qWaitFor([] { return taskManager->tasks().isEmpty(); }, 10000);
        QTest::qVerify(finished, "audio tasks finished", "release the clip gesture fixture",
                       __FILE__, __LINE__);
        if (QTest::currentTestFailed())
            directory.setAutoRemove(false);
    });
    TrackEditorView editor;
    auto *canvas = editor.findChild<TracksGraphicsView *>();
    QVERIFY(canvas);
    const auto discardUnfinishedDrag = qScopeGuard([canvas] { canvas->discardAction(); });

    constexpr int originalStart = 480;
    constexpr int clipLength = 1920;
    constexpr int moveTicks = 480;
    Automation::ClipDraftDto clipDraft;
    clipDraft.type = Automation::ClipDraftDto::Type::Singing;
    clipDraft.properties.name = QStringLiteral("Dragged clip");
    clipDraft.properties.start = originalStart;
    clipDraft.properties.length = clipLength;
    clipDraft.properties.clipLen = clipLength;
    clipDraft.defaultLanguage = QStringLiteral("eng");
    if (audio) {
        clipDraft.type = Automation::ClipDraftDto::Type::Audio;
        clipDraft.audioPath = directory.filePath(QStringLiteral("gesture.wav"));
        QVERIFY(TestSupport::writeWave(clipDraft.audioPath, QVector<float>(40000, 0.1f), 1, 8000));
        clipDraft.hasRealTimeAnchor = true;
        clipDraft.properties.start = 0;
        clipDraft.properties.clipStart = originalStart;
        clipDraft.properties.length = 4800;
        clipDraft.properties.trimStartMs = 500;
        clipDraft.properties.playLengthMs = 2000;
        clipDraft.properties.materialLengthMs = 5000;
    }
    Automation::TrackDraftDto trackDraft;
    trackDraft.name = QStringLiteral("Track gesture");
    trackDraft.defaultLanguage = QStringLiteral("eng");
    trackDraft.clips.append(clipDraft);
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, trackDraft));
    QCOMPARE(context->m_appModel->tracks().size(), 1);
    auto *track = context->m_appModel->tracks().first();
    QCOMPARE(track->clips().count(), 1);
    const auto *clip = *track->clips().begin();
    QVERIFY(clip);
    if (audio) {
        const auto *audioClip = qobject_cast<const AudioClip *>(clip);
        QVERIFY(audioClip && audioClip->hasRealTimeAnchor());
        QTRY_VERIFY(audioClip->audioInfo().frames > 0);
        QTRY_VERIFY(taskManager->tasks().isEmpty());
    }
    const auto clipId = clip->id();
    auto *item = editor.findClipItemById(clipId);
    QVERIFY(item);

    editor.resize(1200, 500);
    canvas->setAnimationEnabled(false);
    editor.show();
    editor.activateWindow();
    canvas->setFocus();
    QTRY_VERIFY(editor.isVisible() && canvas->viewport()->width() > 600);
    QVERIFY(canvas->setViewportScale(2.0, 1.0));
    canvas->setViewportStartTick(0);
    QCoreApplication::processEvents();
    historyManager->reset();

    const auto before = runtime.documentVersion();
    const auto originalRect = item->sceneBoundingRect();
    const auto clipState = [clip] {
        const auto properties = Automation::clipPropertiesDto(*clip);
        return std::tuple{properties.start,           properties.length,
                          properties.clipStart,       properties.clipLen,
                          properties.trimStartMs,     properties.playLengthMs,
                          properties.materialLengthMs};
    };
    const auto originalProperties = clipState();
    const auto originalModelStart = clip->start();
    auto pressPoint = originalRect.center();
    if (edge < 0)
        pressPoint.setX(originalRect.left() + 2);
    else if (edge > 0)
        pressPoint.setX(originalRect.right() - 2);
    const auto press = canvas->mapFromScene(pressPoint);
    const auto deltaPixels = canvas->sceneXForTick(moveTicks) - canvas->sceneXForTick(0);
    const auto release = canvas->mapFromScene(pressPoint + QPointF(deltaPixels, 0));
    const int expectedLeft = originalStart + (edge <= 0 ? moveTicks : 0);
    const int expectedLength = clipLength + edge * moveTicks;
    QVERIFY(canvas->viewport()->rect().contains(press));
    QVERIFY(canvas->viewport()->rect().contains(release));
    QCOMPARE(canvas->itemAt(press), static_cast<QGraphicsItem *>(item));
    QVERIFY(!item->isSelected());

    QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, press);
    QVERIFY(item->isSelected());
    QCOMPARE(item->activeClip(), !audio);
    QCOMPARE(canvas->selectedClipsId(), QList<int>{clipId});
    QCOMPARE(appStatus->selectedClips.get(), QList<int>{clipId});
    QCOMPARE(appStatus->activeClipId.get(), audio ? -1 : clipId);
    QCOMPARE(runtime.documentVersion(), before);

    QMouseEvent move(QEvent::MouseMove, QPointF(release),
                     QPointF(canvas->viewport()->mapToGlobal(release)), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas->viewport(), &move);
    QTRY_COMPARE(item->start() + item->clipStart(), expectedLeft);
    QCOMPARE(item->clipLen(), expectedLength);
    QCOMPARE(clipState(), originalProperties);
    QCOMPARE(item->sceneBoundingRect().left(), originalRect.left() + (edge <= 0 ? deltaPixels : 0));
    QCOMPARE(item->sceneBoundingRect().width(), originalRect.width() + edge * deltaPixels);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(editSessionManager->hasActiveTransaction());

    if (cancel)
        QTest::keyClick(canvas, Qt::Key_Escape);
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, release);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(appStatus->currentEditObject.get(), AppStatus::EditObjectType::None);
    item = editor.findClipItemById(clipId);
    QVERIFY(item);
    QCOMPARE(clip->start() + clip->clipStart(), cancel ? originalStart : expectedLeft);
    QCOMPARE(item->start(), clip->start());
    QCOMPARE(clip->clipLen(), cancel ? clipLength : expectedLength);
    QCOMPARE(item->clipLen(), clip->clipLen());
    QCOMPARE(item->length(), clip->length());
    QCOMPARE(canvas->selectedClipsId(), QList<int>{clipId});
    QCOMPARE(appStatus->selectedClips.get(), QList<int>{clipId});

    if (cancel) {
        QCOMPARE(clipState(), originalProperties);
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(item->sceneBoundingRect(), originalRect);
        QVERIFY(!historyManager->canUndo());
        QVERIFY(!historyManager->canRedo());
        return;
    }

    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    const auto committedProperties = clipState();
    if (audio) {
        const auto *audioClip = qobject_cast<const AudioClip *>(clip);
        QCOMPARE(audioClip->trimStartMs(), edge < 0 ? 1000.0 : 500.0);
        QCOMPARE(audioClip->playLengthMs(), 2000.0 + edge * 500.0);
        QCOMPARE(audioClip->materialLengthMs(), 5000.0);
    }
    QVERIFY(historyManager->canUndo());
    QVERIFY(!historyManager->canRedo());
    const auto undone = runtime.history().undo(commandContext());
    QVERIFY(undone && undone.get().changed);
    QCOMPARE(clipState(), originalProperties);
    item = editor.findClipItemById(clipId);
    QVERIFY(item);
    QCOMPARE(item->start(), originalModelStart);
    QCOMPARE(item->sceneBoundingRect(), originalRect);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 2);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(historyManager->canRedo());
    QVERIFY(runtime.history().redo(commandContext()));
    QCOMPARE(clipState(), committedProperties);
    item = editor.findClipItemById(clipId);
    QVERIFY(item);
    QCOMPARE(item->start() + item->clipStart(), expectedLeft);
    QCOMPARE(item->clipLen(), expectedLength);
}

void ApplicationGuiTests::timelineGesturesSeekAndCommitLoopEdits() {
    auto &runtime = *context->m_coreRuntime;
    const LoopSettings original(true, 480, 960);
    QVERIFY(runtime.playback().setLoop(commandContext(), original));
    historyManager->reset();
    TimelineView ruler;
    ruler.resize(960, 40);
    ruler.setTimeRange(0, 3840);
    ruler.setQuantize(16);
    ruler.setCanEditLoop(true);
    ruler.show();
    ruler.activateWindow();
    QTRY_VERIFY(ruler.isVisible());
    const auto finishPendingEdit =
        qScopeGuard([&] { playbackController->commitLoopSettingsEdit(original); });
    const auto point = [&](const int tick, const int y) {
        return QPoint(qRound(tick * ruler.width() / 3840.0), y);
    };
    const auto moveWithLeftButton = [&](const QPoint &position) {
        QMouseEvent move(QEvent::MouseMove, QPointF(position), QPointF(ruler.mapToGlobal(position)),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&ruler, &move);
    };
    const auto before = runtime.documentVersion();
    QTest::mouseClick(&ruler, Qt::LeftButton, Qt::NoModifier, point(960, ruler.height() - 6));
    QCOMPARE(playbackController->position(), 960.0);
    QCOMPARE(playbackController->lastPosition(), 960.0);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());

    const LoopSettings moved(true, 960, 960);
    QTest::mousePress(&ruler, Qt::LeftButton, Qt::NoModifier, point(960, 5));
    moveWithLeftButton(point(1440, 5));
    QCOMPARE(appStatus->loopSettings.get(), moved);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    QTest::mouseRelease(&ruler, Qt::LeftButton, Qt::NoModifier, point(1440, 5));
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QCOMPARE(appStatus->loopSettings.get(), moved);
    QVERIFY(historyManager->canUndo());

    const auto afterMove = runtime.documentVersion();
    const auto *undoEntry = historyManager->nextUndoEntry();
    QTest::mouseClick(&ruler, Qt::LeftButton, Qt::NoModifier, point(1440, 5));
    QCOMPARE(runtime.documentVersion(), afterMove);
    QCOMPARE(historyManager->nextUndoEntry(), undoEntry);

    QTest::mousePress(&ruler, Qt::LeftButton, Qt::NoModifier, point(moved.end(), 5));
    moveWithLeftButton(point(2400, 5));
    QCOMPARE(appStatus->loopSettings.get(), LoopSettings(true, 960, 1440));
    QCOMPARE(runtime.documentVersion(), afterMove);
    QTest::mouseRelease(&ruler, Qt::LeftButton, Qt::NoModifier, point(2400, 5));
    QCOMPARE(runtime.documentVersion().revision, afterMove.revision + 1);
    const auto playback = runtime.playback().getPlayback(runtime.documentVersion().documentId);
    QVERIFY(playback);
    QCOMPARE(playback.get().loop, LoopSettings(true, 960, 1440));
    historyManager->undo();
    QCOMPARE(appStatus->loopSettings.get(), moved);
    historyManager->undo();
    QCOMPARE(appStatus->loopSettings.get(), original);
    QVERIFY(!historyManager->canUndo());
}
