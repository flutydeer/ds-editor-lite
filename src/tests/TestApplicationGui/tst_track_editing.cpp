#include "tst_application_gui.h"
#include "UI/Views/Common/EditorPointerUtils.h"
#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/TrackController.h"
#include "Controller/PlaybackController.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "TestSupport/WaveFixture.h"
#include "TestSupport/MainWindowFixture.h"
#include "Global/TracksEditorGlobal.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoKeyboardView.h"
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
#include <QImage>
#include <QMouseEvent>
#include <QPixmap>
#include <QScopeGuard>
#include <QScrollBar>
#include <QtTest/QTest>

#include <tuple>

void ApplicationGuiTests::trackClipDragContinuesDuringEdgeScrollingAndStopsOnFinish_data() {
    QTest::addColumn<bool>("atProjectTail");
    QTest::newRow("inside-project") << false;
    QTest::newRow("past-project-tail") << true;
}

void ApplicationGuiTests::trackClipDragContinuesDuringEdgeScrollingAndStopsOnFinish() {
    QFETCH(bool, atProjectTail);
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    TrackEditorView editor;
    const auto clearParent = qScopeGuard([] { trackController->setParentWidget(nullptr); });
    Automation::TrackDraftDto trackDraft;
    Automation::ClipDraftDto clipDraft;
    clipDraft.type = Automation::ClipDraftDto::Type::Singing;
    clipDraft.properties.start = atProjectTail ? appStatus->projectEditableLength.get() - 960 : 480;
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
    QVERIFY(canvas->setViewportScale(atProjectTail ? 1.0 : 3.0, 1.0));
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto originalTail = appStatus->projectEditableLength.get();
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
        if (atProjectTail)
            canvas->setHorizontalBarValue(canvas->horizontalScrollBar()->maximum());
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
        if (atProjectTail)
            QTRY_VERIFY(item->start() > originalTail);
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
    QTest::addColumn<bool>("touch");
    QTest::newRow("singing-move") << false << 0 << false << false;
    QTest::newRow("singing-cancel-move") << false << 0 << true << false;
    QTest::newRow("singing-trim-left") << false << -1 << false << false;
    QTest::newRow("singing-extend-right") << false << 1 << false << false;
    QTest::newRow("audio-move") << true << 0 << false << false;
    QTest::newRow("audio-trim-left") << true << -1 << false << false;
    QTest::newRow("audio-extend-right") << true << 1 << false << false;
    QTest::newRow("audio-cancel-trim") << true << -1 << true << false;
    QTest::newRow("touch-move") << false << 0 << false << true;
    QTest::newRow("touch-cancel-move") << false << 0 << true << true;
}

void ApplicationGuiTests::trackClipDragCommitsOrCancels() {
    QFETCH(bool, audio);
    QFETCH(int, edge);
    QFETCH(bool, cancel);
    QFETCH(bool, touch);
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
    const auto discardUnfinishedDrag = qScopeGuard([canvas, touch] {
        if (touch) {
            QEvent deactivate(QEvent::WindowDeactivate);
            QApplication::sendEvent(canvas, &deactivate);
        }
        canvas->discardAction();
    });

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

    auto *touchDevice = touch ? QTest::createTouchDevice() : nullptr;
    const auto before = runtime.documentVersion();
    if (touch) {
        const auto beforePan = TestSupport::projectSnapshot(*context->m_appModel);
        const auto from = canvas->viewport()->rect().center() + QPoint(0, 100);
        const auto to = from + QPoint(-60, 0);
        const auto left = canvas->horizontalBarValue();
        QTest::touchEvent(canvas->viewport(), touchDevice).press(0, from);
        QTest::touchEvent(canvas->viewport(), touchDevice).move(0, from + QPoint(-25, 0));
        QTest::qWait(20);
        QTest::touchEvent(canvas->viewport(), touchDevice).move(0, to);
        QTRY_VERIFY(canvas->horizontalBarValue() > left);
        QVERIFY(!editSessionManager->hasActiveTransaction());
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforePan);
        QVERIFY(!historyManager->canUndo());
        QTest::touchEvent(canvas->viewport(), touchDevice).release(0, to);
        QEvent deactivate(QEvent::WindowDeactivate);
        QApplication::sendEvent(canvas, &deactivate);
        QVERIFY(!EditorPointer::isTouchStreamActive());
        canvas->setViewportStartTick(0);
    }
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

    if (touch) {
        QTest::touchEvent(canvas->viewport(), touchDevice).press(0, press);
        QTest::touchEvent(canvas->viewport(), touchDevice).release(0, press);
        QTRY_VERIFY(item->isSelected());
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(clipState(), originalProperties);
        QVERIFY(!historyManager->canUndo());
    }
    if (touch)
        QTest::touchEvent(canvas->viewport(), touchDevice).press(0, press);
    else
        QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, press);
    QCOMPARE(runtime.documentVersion(), before);

    if (touch) {
        QTest::touchEvent(canvas->viewport(), touchDevice).move(0, release);
    } else {
        QMouseEvent move(QEvent::MouseMove, QPointF(release),
                         QPointF(canvas->viewport()->mapToGlobal(release)), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(canvas->viewport(), &move);
    }
    QVERIFY(item->isSelected());
    QCOMPARE(item->activeClip(), !audio);
    QCOMPARE(canvas->selectedClipsId(), QList<int>{clipId});
    QCOMPARE(appStatus->selectedClips.get(), QList<int>{clipId});
    QCOMPARE(appStatus->activeClipId.get(), audio ? -1 : clipId);
    QTRY_COMPARE(item->start() + item->clipStart(), expectedLeft);
    QCOMPARE(item->clipLen(), expectedLength);
    QCOMPARE(clipState(), originalProperties);
    QCOMPARE(item->sceneBoundingRect().left(), originalRect.left() + (edge <= 0 ? deltaPixels : 0));
    QCOMPARE(item->sceneBoundingRect().width(), originalRect.width() + edge * deltaPixels);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(editSessionManager->hasActiveTransaction());

    if (touch) {
        if (cancel) {
            QEvent deactivate(QEvent::WindowDeactivate);
            QApplication::sendEvent(canvas, &deactivate);
        }
        QTest::touchEvent(canvas->viewport(), touchDevice).release(0, release);
        QVERIFY(!EditorPointer::isTouchStreamActive());
    } else {
        if (cancel)
            QTest::keyClick(canvas, Qt::Key_Escape);
        QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, release);
    }
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

void ApplicationGuiTests::timelineGesturesSeekAndCommitLoopEdits_data() {
    QTest::addColumn<bool>("leftEdge");
    QTest::newRow("resize-left") << true;
    QTest::newRow("resize-right") << false;
}

void ApplicationGuiTests::timelineGesturesSeekAndCommitLoopEdits() {
    QFETCH(bool, leftEdge);
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
    const auto enabledImage = ruler.grab().toImage();
    QVERIFY(!enabledImage.isNull());
    QVERIFY(runtime.playback().setLoop(commandContext(), LoopSettings(false, 480, 960)));
    const auto disabledImage = ruler.grab().toImage();
    QVERIFY(!disabledImage.isNull());
    const auto colorAt = [&](const QImage &image, const int y) {
        const auto location = QPointF(point(960, y)) * image.devicePixelRatio();
        return image.pixelColor(location.toPoint());
    };
    QVERIFY(colorAt(enabledImage, 1) != colorAt(disabledImage, 1));
    QVERIFY(colorAt(enabledImage, 6) != colorAt(disabledImage, 6));
    QVERIFY(colorAt(disabledImage, 1) != colorAt(disabledImage, 6));
    QVERIFY(runtime.playback().setLoop(commandContext(), original));
    historyManager->reset();
    const auto before = runtime.documentVersion();
    QTest::mouseClick(&ruler, Qt::LeftButton, Qt::NoModifier, point(960, ruler.height() - 6));
    QCOMPARE(playbackController->position(), 960.0);
    QCOMPARE(playbackController->lastPosition(), 960.0);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());

    const auto seekY = ruler.height() - 6;
    QTest::mousePress(&ruler, Qt::LeftButton, Qt::NoModifier, point(960, seekY));
    moveWithLeftButton(point(1200, seekY));
    QCOMPARE(playbackController->position(), 1200.0);
    QCOMPARE(playbackController->lastPosition(), 1200.0);
    QCOMPARE(appStatus->loopSettings.get(), original);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    moveWithLeftButton(point(1440, seekY));
    QTest::mouseRelease(&ruler, Qt::LeftButton, Qt::NoModifier, point(1440, seekY));
    QCOMPARE(playbackController->position(), 1440.0);
    QCOMPARE(playbackController->lastPosition(), 1440.0);
    QCOMPARE(appStatus->loopSettings.get(), original);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(!editSessionManager->hasActiveTransaction());

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

    const LoopSettings resized(true, leftEdge ? 480 : 960, 1440);
    const auto resizeStart = point(leftEdge ? moved.start : moved.end(), 5);
    const auto resizeEnd = point(leftEdge ? resized.start : resized.end(), 5);
    QTest::mousePress(&ruler, Qt::LeftButton, Qt::NoModifier, resizeStart);
    moveWithLeftButton(resizeEnd);
    QCOMPARE(appStatus->loopSettings.get(), resized);
    QCOMPARE(runtime.documentVersion(), afterMove);
    QCOMPARE(historyManager->nextUndoEntry(), undoEntry);
    QTest::mouseRelease(&ruler, Qt::LeftButton, Qt::NoModifier, resizeEnd);
    QCOMPARE(runtime.documentVersion().revision, afterMove.revision + 1);
    const auto playback = runtime.playback().getPlayback(runtime.documentVersion().documentId);
    QVERIFY(playback);
    QCOMPARE(playback.get().loop, resized);
    historyManager->undo();
    QCOMPARE(appStatus->loopSettings.get(), moved);
    historyManager->undo();
    QCOMPARE(appStatus->loopSettings.get(), original);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::activeClipMoveRebindsThePianoTrackColor() {
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    TestSupport::MainWindowFixture host;
    Automation::ClipDraftDto clipDraft;
    clipDraft.type = Automation::ClipDraftDto::Type::Singing;
    clipDraft.properties.start = 480;
    clipDraft.properties.length = 1920;
    clipDraft.properties.clipLen = 1920;
    Automation::TrackDraftDto sourceDraft;
    sourceDraft.name = QStringLiteral("Source track");
    sourceDraft.resolveColorIndex = false;
    sourceDraft.colorIndex = 1;
    sourceDraft.clips.append(clipDraft);
    Automation::TrackDraftDto targetDraft;
    targetDraft.name = QStringLiteral("Target track");
    targetDraft.resolveColorIndex = false;
    targetDraft.colorIndex = 2;
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, sourceDraft));
    QVERIFY(runtime.project().insertTrack(commandContext(), 1, targetDraft));
    auto *source = context->m_appModel->tracks().at(0);
    auto *target = context->m_appModel->tracks().at(1);
    auto *clip = *source->clips().begin();
    host.show();
    if (QTest::currentTestFailed())
        return;
    auto *editor = host.window->findChild<TrackEditorView *>();
    auto *canvas = host.window->findChild<TracksGraphicsView *>();
    auto *keyboard = host.window->findChild<PianoKeyboardView *>();
    QVERIFY(editor && canvas && keyboard);
    canvas->setAnimationEnabled(false);
    QVERIFY(canvas->setViewportScale(2.0, 1.0));
    canvas->setViewportStartTick(0);
    QCoreApplication::processEvents();
    auto *item = editor->findClipItemById(clip->id());
    QVERIFY(item);
    const auto press = canvas->mapFromScene(item->sceneBoundingRect().center());
    const auto release = canvas->mapFromScene(item->sceneBoundingRect().center() +
                                              QPointF(0, TracksEditorGlobal::trackHeight));
    QVERIFY(canvas->viewport()->rect().contains(press));
    QVERIFY(canvas->viewport()->rect().contains(release));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, press);
    QCOMPARE(appStatus->activeClipId.get(), clip->id());
    QTRY_VERIFY(keyboard->isVisible());
    const auto keyboardImage = [&] {
        const auto point = keyboard->rect().center();
        QMouseEvent hover(QEvent::MouseMove, QPointF(point), QPointF(keyboard->mapToGlobal(point)),
                          Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(keyboard, &hover);
        return keyboard->grab().toImage();
    };
    const auto originalKeyboard = keyboardImage();
    historyManager->reset();
    const auto before = TestSupport::projectSnapshot(*context->m_appModel);
    const auto version = runtime.documentVersion();
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, press);
    const auto cancelOnFailure = qScopeGuard([&] {
        if (editSessionManager->hasActiveTransaction()) {
            canvas->discardAction();
            QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, release);
        }
    });
    QMouseEvent move(QEvent::MouseMove, QPointF(release),
                     QPointF(canvas->viewport()->mapToGlobal(release)), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas->viewport(), &move);
    QVERIFY(editSessionManager->hasActiveTransaction());
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), before);
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, release);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(source->clips().count(), 0);
    QCOMPARE(target->clips().count(), 1);
    QCOMPARE(*target->clips().begin(), clip);
    QCOMPARE(runtime.documentVersion().revision, version.revision + 1);
    const auto movedKeyboard = keyboardImage();
    QVERIFY(movedKeyboard != originalKeyboard);
    QVERIFY(
        runtime.project().setTrackColor(commandContext(), Automation::TrackId(source->id()), 3));
    QCOMPARE(keyboardImage(), movedKeyboard);
    QVERIFY(
        runtime.project().setTrackColor(commandContext(), Automation::TrackId(target->id()), 4));
    const auto recoloredKeyboard = keyboardImage();
    QVERIFY(recoloredKeyboard != movedKeyboard);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(keyboardImage(), movedKeyboard);
    QVERIFY(runtime.history().undo(commandContext()));
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), before);
    QCOMPARE(keyboardImage(), originalKeyboard);
    QVERIFY(runtime.history().redo(commandContext()));
    QCOMPARE(keyboardImage(), movedKeyboard);
    QCOMPARE(target->clips().count(), 1);
    QCOMPARE(*target->clips().begin(), clip);
    QVERIFY(runtime.history().redo(commandContext()));
    QCOMPARE(keyboardImage(), movedKeyboard);
    QVERIFY(runtime.history().redo(commandContext()));
    QCOMPARE(keyboardImage(), recoloredKeyboard);
}
