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

#include <algorithm>
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
    // Audio clips become the active clip too: the bottom panel hosts a read-only
    // audio view and its name editor for them
    QCOMPARE(item->activeClip(), true);
    QCOMPARE(canvas->selectedClipsId(), QList<int>{clipId});
    QCOMPARE(appStatus->selectedClips.get(), QList<int>{clipId});
    QCOMPARE(appStatus->activeClipId.get(), clipId);
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

void ApplicationGuiTests::activeClipMoveRebindsThePianoTrackColor_data() {
    QTest::addColumn<QSize>("maximumWindowSize");
    QTest::newRow("default-window") << QSize(1200, 900);
    QTest::newRow("small-desktop") << QSize(944, 565);
}

void ApplicationGuiTests::activeClipMoveRebindsThePianoTrackColor() {
    QFETCH(QSize, maximumWindowSize);
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    TestSupport::MainWindowFixture host;
    host.window->setMaximumSize(maximumWindowSize);
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
    // Use compact track rows when the native desktop constrains the window.
    QVERIFY(canvas->setViewportScale(2.0, canvas->scaleYMin()));
    canvas->setViewportStartTick(0);
    canvas->setVerticalBarValue(0);
    QCoreApplication::processEvents();
    auto *item = editor->findClipItemById(clip->id());
    QVERIFY(item);
    const auto rowHeight = TracksEditorGlobal::trackHeight * canvas->scaleY();
    // The target row can be partially visible, so its center need not be a valid drop point.
    const auto pressArea =
        item->sceneBoundingRect().intersected(canvas->visibleRect().adjusted(0, 0, 0, -rowHeight));
    QVERIFY(!pressArea.isEmpty());
    const auto press = canvas->mapFromScene(pressArea.center());
    const auto release = canvas->mapFromScene(pressArea.center() + QPointF(0, rowHeight));
    const auto geometryDiagnostic =
        QStringLiteral("Viewport %1x%2, press (%3, %4), release (%5, %6), scaleY %7")
            .arg(canvas->viewport()->width())
            .arg(canvas->viewport()->height())
            .arg(press.x())
            .arg(press.y())
            .arg(release.x())
            .arg(release.y())
            .arg(canvas->scaleY());
    QVERIFY2(canvas->viewport()->rect().contains(press), qPrintable(geometryDiagnostic));
    QVERIFY2(canvas->viewport()->rect().contains(release), qPrintable(geometryDiagnostic));
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

namespace {

    QList<int> canonicalClipIds(const QList<int> &ids) {
        auto result = ids;
        std::sort(result.begin(), result.end());
        return result;
    }

    void appendEmptyTrack(Automation::CoreRuntime &runtime,
                          const Automation::CommandContext &context, const QString &name) {
        Automation::TrackDraftDto draft;
        draft.name = name;
        draft.defaultLanguage = QStringLiteral("eng");
        runtime.project().insertTrack(context, appModel->tracks().size(), draft);
    }

    void sendLeftDrag(QWidget *target, const QPoint &from, const QPoint &to,
                      Qt::KeyboardModifiers modifiers) {
        QTest::mousePress(target, Qt::LeftButton, Qt::NoModifier, from);
        QMouseEvent move(QEvent::MouseMove, QPointF(to), QPointF(target->mapToGlobal(to)),
                         Qt::NoButton, Qt::LeftButton, modifiers);
        QApplication::sendEvent(target, &move);
        QTest::mouseRelease(target, Qt::LeftButton, modifiers, to);
    }

}

void ApplicationGuiTests::trackClipGroupDragCommitsAtomically_data() {
    QTest::addColumn<bool>("audioAnchor");
    QTest::newRow("singing-block") << false;
    QTest::newRow("audio-anchor") << true;
}

void ApplicationGuiTests::trackClipGroupDragCommitsAtomically() {
    QFETCH(bool, audioAnchor);
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    TrackEditorView editor;
    auto *canvas = editor.findChild<TracksGraphicsView *>();
    QVERIFY(canvas);
    const auto clearDocument = qScopeGuard([&] {
        trackController->setParentWidget(nullptr);
        canvas->discardAction();
        runtime.documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false));
        const auto finished = QTest::qWaitFor([] { return taskManager->tasks().isEmpty(); }, 10000);
        QTest::qVerify(finished, "audio tasks finished", "release the clip block fixture", __FILE__,
                       __LINE__);
    });

    Automation::TrackDraftDto firstTrack;
    firstTrack.name = QStringLiteral("Block source");
    firstTrack.defaultLanguage = QStringLiteral("eng");
    Automation::ClipDraftDto firstClipDraft;
    firstClipDraft.type = Automation::ClipDraftDto::Type::Singing;
    firstClipDraft.defaultLanguage = QStringLiteral("eng");
    firstClipDraft.properties.name = QStringLiteral("Block A");
    firstClipDraft.properties.start = 480;
    firstClipDraft.properties.length = 1920;
    firstClipDraft.properties.clipLen = 1920;
    firstTrack.clips.append(firstClipDraft);
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, firstTrack));

    Automation::TrackDraftDto secondTrack;
    secondTrack.name = QStringLiteral("Block anchor");
    secondTrack.defaultLanguage = QStringLiteral("eng");
    Automation::ClipDraftDto anchorDraft;
    anchorDraft.defaultLanguage = QStringLiteral("eng");
    anchorDraft.properties.name = QStringLiteral("Block B");
    anchorDraft.properties.start = 960;
    anchorDraft.properties.length = 1920;
    anchorDraft.properties.clipLen = 1920;
    if (audioAnchor) {
        anchorDraft.type = Automation::ClipDraftDto::Type::Audio;
        anchorDraft.audioPath = directory.filePath(QStringLiteral("block.wav"));
        QVERIFY(
            TestSupport::writeWave(anchorDraft.audioPath, QVector<float>(40000, 0.1f), 1, 8000));
        anchorDraft.hasRealTimeAnchor = true;
        anchorDraft.properties.start = 0;
        anchorDraft.properties.clipStart = 960;
        anchorDraft.properties.length = 3840;
        anchorDraft.properties.trimStartMs = 500;
        anchorDraft.properties.playLengthMs = 2000;
        anchorDraft.properties.materialLengthMs = 5000;
    } else {
        anchorDraft.type = Automation::ClipDraftDto::Type::Singing;
    }
    secondTrack.clips.append(anchorDraft);
    QVERIFY(runtime.project().insertTrack(commandContext(), 1, secondTrack));
    appendEmptyTrack(runtime, commandContext(), QStringLiteral("Block target"));
    QCOMPARE(appModel->tracks().size(), 3);

    auto *blockClip = *appModel->tracks().at(0)->clips().begin();
    auto *anchorClip = *appModel->tracks().at(1)->clips().begin();
    QVERIFY(blockClip && anchorClip);
    const auto blockClipId = blockClip->id();
    const auto anchorClipId = anchorClip->id();
    if (audioAnchor) {
        const auto *audio = qobject_cast<const AudioClip *>(anchorClip);
        QVERIFY(audio && audio->hasRealTimeAnchor());
        QTRY_VERIFY(audio->audioInfo().frames > 0);
        QTRY_VERIFY(taskManager->tasks().isEmpty());
    }

    editor.resize(1200, 560);
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
    const auto originalBlockStart = blockClip->start() + blockClip->clipStart();
    const auto originalAnchorStart = anchorClip->start() + anchorClip->clipStart();
    constexpr int originalBlockTrack = 0;
    constexpr int originalAnchorTrack = 1;

    auto *blockItem = editor.findClipItemById(blockClipId);
    auto *anchorItem = editor.findClipItemById(anchorClipId);
    QVERIFY(blockItem && anchorItem);
    const auto pointFor = [canvas](const AbstractClipView *item) {
        return canvas->mapFromScene(item->sceneBoundingRect().center());
    };
    QVERIFY(canvas->viewport()->rect().contains(pointFor(blockItem)));
    QVERIFY(canvas->viewport()->rect().contains(pointFor(anchorItem)));

    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::ControlModifier, pointFor(blockItem));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::ControlModifier,
                      pointFor(anchorItem));
    QCOMPARE(canonicalClipIds(appStatus->selectedClips.get()),
             canonicalClipIds({blockClipId, anchorClipId}));

    constexpr int moveTicks = 480;
    const auto rowPitch = TracksEditorGlobal::trackHeight * canvas->scaleY();
    const auto tickDelta = canvas->sceneXForTick(moveTicks) - canvas->sceneXForTick(0);
    const auto from = canvas->mapFromScene(anchorItem->sceneBoundingRect().center());
    const auto to = canvas->mapFromScene(anchorItem->sceneBoundingRect().center() +
                                         QPointF(tickDelta, rowPitch));
    QCOMPARE(canvas->itemAt(from), static_cast<QGraphicsItem *>(anchorItem));

    QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, from);
    // Pressing a clip that belongs to the block must keep the block, not collapse it.
    QCOMPARE(canonicalClipIds(appStatus->selectedClips.get()),
             canonicalClipIds({blockClipId, anchorClipId}));
    QVERIFY(editSessionManager->hasActiveTransaction());
    {
        QMouseEvent move(QEvent::MouseMove, QPointF(to),
                         QPointF(canvas->viewport()->mapToGlobal(to)), Qt::NoButton, Qt::LeftButton,
                         Qt::AltModifier);
        QApplication::sendEvent(canvas->viewport(), &move);
    }
    QCOMPARE(blockItem->start() + blockItem->clipStart(), originalBlockStart + moveTicks);
    QCOMPARE(anchorItem->start() + anchorItem->clipStart(), originalAnchorStart + moveTicks);
    QCOMPARE(blockItem->trackIndex(), originalBlockTrack + 1);
    QCOMPARE(anchorItem->trackIndex(), originalAnchorTrack + 1);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());

    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::AltModifier, to);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(blockClip->start() + blockClip->clipStart(), originalBlockStart + moveTicks);
    QCOMPARE(anchorClip->start() + anchorClip->clipStart(), originalAnchorStart + moveTicks);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    if (audioAnchor) {
        const auto *audio = qobject_cast<const AudioClip *>(anchorClip);
        QVERIFY(audio);
        QCOMPARE(audio->trimStartMs(), 500.0);
        QCOMPARE(audio->playLengthMs(), 2000.0);
        QCOMPARE(audio->materialLengthMs(), 5000.0);
    }
    QVERIFY(historyManager->canUndo());
    blockItem = editor.findClipItemById(blockClipId);
    anchorItem = editor.findClipItemById(anchorClipId);
    QVERIFY(blockItem && anchorItem);
    QCOMPARE(blockItem->start(), blockClip->start());
    QCOMPARE(anchorItem->start(), anchorClip->start());
    QCOMPARE(blockItem->trackIndex(), originalBlockTrack + 1);
    QCOMPARE(anchorItem->trackIndex(), originalAnchorTrack + 1);

    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(blockClip->start() + blockClip->clipStart(), originalBlockStart);
    QCOMPARE(anchorClip->start() + anchorClip->clipStart(), originalAnchorStart);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(historyManager->canRedo());
    QVERIFY(runtime.history().redo(commandContext()));
    QCOMPARE(blockClip->start() + blockClip->clipStart(), originalBlockStart + moveTicks);
    QCOMPARE(anchorClip->start() + anchorClip->clipStart(), originalAnchorStart + moveTicks);
}

void ApplicationGuiTests::trackClipGroupTrimCommitsAtomically_data() {
    QTest::addColumn<int>("edge");
    QTest::newRow("right-edge") << 1;
    QTest::newRow("left-edge") << -1;
}

void ApplicationGuiTests::trackClipGroupTrimCommitsAtomically() {
    QFETCH(int, edge);
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));

    TrackEditorView editor;
    auto *canvas = editor.findChild<TracksGraphicsView *>();
    QVERIFY(canvas);
    const auto clearDocument = qScopeGuard([&] {
        trackController->setParentWidget(nullptr);
        canvas->discardAction();
        runtime.documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false));
    });

    Automation::TrackDraftDto firstTrack;
    firstTrack.name = QStringLiteral("Trim source");
    firstTrack.defaultLanguage = QStringLiteral("eng");
    Automation::ClipDraftDto firstClipDraft;
    firstClipDraft.type = Automation::ClipDraftDto::Type::Singing;
    firstClipDraft.defaultLanguage = QStringLiteral("eng");
    firstClipDraft.properties.start = 960;
    firstClipDraft.properties.length = 4800;
    firstClipDraft.properties.clipStart = 480;
    firstClipDraft.properties.clipLen = 1920;
    firstTrack.clips.append(firstClipDraft);
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, firstTrack));

    Automation::TrackDraftDto secondTrack;
    secondTrack.name = QStringLiteral("Trim peer");
    secondTrack.defaultLanguage = QStringLiteral("eng");
    Automation::ClipDraftDto secondClipDraft;
    secondClipDraft.type = Automation::ClipDraftDto::Type::Singing;
    secondClipDraft.defaultLanguage = QStringLiteral("eng");
    secondClipDraft.properties.start = 1440;
    secondClipDraft.properties.length = 4800;
    secondClipDraft.properties.clipStart = 480;
    secondClipDraft.properties.clipLen = 1920;
    secondTrack.clips.append(secondClipDraft);
    QVERIFY(runtime.project().insertTrack(commandContext(), 1, secondTrack));

    auto *firstClip = *appModel->tracks().at(0)->clips().begin();
    auto *secondClip = *appModel->tracks().at(1)->clips().begin();
    QVERIFY(firstClip && secondClip);
    const auto firstClipId = firstClip->id();
    const auto secondClipId = secondClip->id();
    const auto originalFirstLen = firstClip->clipLen();
    const auto originalSecondLen = secondClip->clipLen();

    editor.resize(1200, 560);
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

    auto *firstItem = editor.findClipItemById(firstClipId);
    auto *secondItem = editor.findClipItemById(secondClipId);
    QVERIFY(firstItem && secondItem);
    const auto pointFor = [canvas](const AbstractClipView *item) {
        return canvas->mapFromScene(item->sceneBoundingRect().center());
    };
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::ControlModifier, pointFor(firstItem));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::ControlModifier,
                      pointFor(secondItem));
    QCOMPARE(canonicalClipIds(appStatus->selectedClips.get()),
             canonicalClipIds({firstClipId, secondClipId}));

    constexpr int trimTicks = 240;
    const auto tickDelta = canvas->sceneXForTick(trimTicks) - canvas->sceneXForTick(0);
    const auto edgeScene = QPointF(edge > 0 ? secondItem->sceneBoundingRect().right() - 2
                                            : secondItem->sceneBoundingRect().left() + 2,
                                   secondItem->sceneBoundingRect().center().y());
    const auto from = canvas->mapFromScene(edgeScene);
    const auto to = canvas->mapFromScene(edgeScene + QPointF(edge * tickDelta, 0));
    QVERIFY(canvas->viewport()->rect().contains(from));

    QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, from);
    // The edge press must keep the block: trimming one clip trims the whole selection.
    QCOMPARE(canonicalClipIds(appStatus->selectedClips.get()),
             canonicalClipIds({firstClipId, secondClipId}));
    {
        QMouseEvent move(QEvent::MouseMove, QPointF(to),
                         QPointF(canvas->viewport()->mapToGlobal(to)), Qt::NoButton, Qt::LeftButton,
                         Qt::AltModifier);
        QApplication::sendEvent(canvas->viewport(), &move);
    }
    QCOMPARE(secondItem->clipLen(), originalSecondLen + trimTicks);
    QCOMPARE(firstItem->clipLen(), originalFirstLen + trimTicks);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::AltModifier, to);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(firstClip->clipLen(), originalFirstLen + trimTicks);
    QCOMPARE(secondClip->clipLen(), originalSecondLen + trimTicks);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QVERIFY(historyManager->canUndo());
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(firstClip->clipLen(), originalFirstLen);
    QCOMPARE(secondClip->clipLen(), originalSecondLen);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::trackClipSelectionModifiersAndNudge_data() {
    QTest::addColumn<bool>("rangeSelect");
    QTest::addColumn<int>("editorWidth");
    for (const auto width : {800, 1400}) {
        QTest::newRow(qPrintable(QStringLiteral("shift-range-%1").arg(width))) << true << width;
        QTest::newRow(qPrintable(QStringLiteral("ctrl-toggle-%1").arg(width))) << false << width;
    }
}

void ApplicationGuiTests::trackClipSelectionModifiersAndNudge() {
    QFETCH(bool, rangeSelect);
    QFETCH(int, editorWidth);
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));

    TrackEditorView editor;
    auto *canvas = editor.findChild<TracksGraphicsView *>();
    QVERIFY(canvas);
    const auto clearDocument = qScopeGuard([&] {
        trackController->setParentWidget(nullptr);
        canvas->discardAction();
        runtime.documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false));
    });

    Automation::TrackDraftDto trackDraft;
    trackDraft.name = QStringLiteral("Nudge track");
    trackDraft.defaultLanguage = QStringLiteral("eng");
    for (int index = 0; index < 3; ++index) {
        Automation::ClipDraftDto clipDraft;
        clipDraft.type = Automation::ClipDraftDto::Type::Singing;
        clipDraft.defaultLanguage = QStringLiteral("eng");
        clipDraft.properties.name = QStringLiteral("Nudge %1").arg(index);
        clipDraft.properties.start = 480 + index * 1920;
        clipDraft.properties.length = 960;
        clipDraft.properties.clipLen = 960;
        trackDraft.clips.append(clipDraft);
    }
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, trackDraft));
    appendEmptyTrack(runtime, commandContext(), QStringLiteral("Nudge target"));
    QCOMPARE(appModel->tracks().size(), 2);

    QList<Clip *> clips;
    for (auto *clip : appModel->tracks().at(0)->clips())
        clips.append(clip);
    QCOMPARE(clips.size(), 3);
    const auto firstId = clips.at(0)->id();
    const auto lastId = clips.at(2)->id();

    editor.resize(editorWidth, 560);
    canvas->setAnimationEnabled(false);
    editor.show();
    editor.activateWindow();
    canvas->setFocus();
    QTRY_VERIFY(editor.isVisible() && canvas->viewport()->isVisible());
    // Native window systems may constrain the requested window size to the screen.
    // Fit the fixture's clips to the actual canvas instead of requiring a desktop width.
    constexpr int visibleTicks = 5760;
    const auto availableWidth = canvas->viewport()->width() -
                                TracksEditorGlobal::trackViewLeftMargin - 20;
    QVERIFY(availableWidth > 0);
    const auto scale = availableWidth * static_cast<double>(AppGlobal::ticksPerQuarterNote) /
                       (visibleTicks * TracksEditorGlobal::pixelsPerQuarterNote);
    QVERIFY(canvas->setViewportScale(std::min(1.5, scale), 1.0));
    canvas->setViewportStartTick(0);
    QCoreApplication::processEvents();
    historyManager->reset();

    auto *firstItem = editor.findClipItemById(firstId);
    auto *lastItem = editor.findClipItemById(lastId);
    QVERIFY(firstItem && lastItem);
    const auto pointFor = [canvas](const AbstractClipView *item) {
        return canvas->mapFromScene(item->sceneBoundingRect().center());
    };
    QVERIFY(canvas->viewport()->rect().contains(pointFor(firstItem)));
    QVERIFY(canvas->viewport()->rect().contains(pointFor(lastItem)));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, pointFor(firstItem));
    QCOMPARE(canonicalClipIds(appStatus->selectedClips.get()), canonicalClipIds({firstId}));
    const auto modifier = rangeSelect ? Qt::ShiftModifier : Qt::ControlModifier;
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, modifier, pointFor(lastItem));
    const auto expectedSelection =
        rangeSelect ? QList<int>{firstId, clips.at(1)->id(), lastId} : QList<int>{firstId, lastId};
    QCOMPARE(canonicalClipIds(appStatus->selectedClips.get()), canonicalClipIds(expectedSelection));

    const auto visibleStartOf = [](const Clip *clip) { return clip->start() + clip->clipStart(); };
    const auto originalStarts = [&] {
        QList<int> starts;
        for (const auto clipId : expectedSelection)
            starts.append(visibleStartOf(appModel->findClipById(clipId)));
        return starts;
    }();

    // Arrow keys move the whole selection by one WYSIWYG grid step.
    canvas->setFocus();
    const auto step = canvas->snapStep(false, originalStarts.constFirst());
    QVERIFY(step > 1);
    auto baseline = runtime.documentVersion();
    QTest::keyClick(canvas, Qt::Key_Right);
    QCOMPARE(runtime.documentVersion().revision, baseline.revision + 1);
    for (qsizetype index = 0; index < expectedSelection.size(); ++index) {
        QCOMPARE(visibleStartOf(appModel->findClipById(expectedSelection.at(index))),
                 originalStarts.at(index) + step);
    }
    QVERIFY(runtime.history().undo(commandContext()));
    for (qsizetype index = 0; index < expectedSelection.size(); ++index) {
        QCOMPARE(visibleStartOf(appModel->findClipById(expectedSelection.at(index))),
                 originalStarts.at(index));
    }

    // Arrow keys also move the block down one track.
    canvas->setFocus();
    baseline = runtime.documentVersion();
    QTest::keyClick(canvas, Qt::Key_Down);
    QCOMPARE(runtime.documentVersion().revision, baseline.revision + 1);
    for (const auto clipId : expectedSelection) {
        Track *track = nullptr;
        QVERIFY(appModel->findClipById(clipId, track));
        QCOMPARE(static_cast<int>(appModel->tracks().indexOf(track)), 1);
    }
    QVERIFY(runtime.history().undo(commandContext()));
    for (const auto clipId : expectedSelection) {
        Track *track = nullptr;
        QVERIFY(appModel->findClipById(clipId, track));
        QCOMPARE(static_cast<int>(appModel->tracks().indexOf(track)), 0);
    }

    // A plain click on a clip that belongs to the selection collapses it to that clip.
    baseline = runtime.documentVersion();
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, pointFor(lastItem));
    QCOMPARE(canonicalClipIds(appStatus->selectedClips.get()), canonicalClipIds({lastId}));
    QCOMPARE(runtime.documentVersion(), baseline);
}

void ApplicationGuiTests::trackClipRangeAnchorClearsWithTheSelection_data() {
    QTest::addColumn<int>("editorWidth");
    QTest::newRow("compact") << 800;
    QTest::newRow("wide") << 1400;
}

void ApplicationGuiTests::trackClipRangeAnchorClearsWithTheSelection() {
    QFETCH(int, editorWidth);
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));

    TrackEditorView editor;
    auto *canvas = editor.findChild<TracksGraphicsView *>();
    QVERIFY(canvas);
    const auto clearDocument = qScopeGuard([&] {
        trackController->setParentWidget(nullptr);
        canvas->discardAction();
        runtime.documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false));
    });

    Automation::TrackDraftDto trackDraft;
    trackDraft.name = QStringLiteral("Anchor track");
    trackDraft.defaultLanguage = QStringLiteral("eng");
    for (int index = 0; index < 3; ++index) {
        Automation::ClipDraftDto clipDraft;
        clipDraft.type = Automation::ClipDraftDto::Type::Singing;
        clipDraft.defaultLanguage = QStringLiteral("eng");
        clipDraft.properties.name = QStringLiteral("Anchor %1").arg(index);
        clipDraft.properties.start = 480 + index * 1920;
        clipDraft.properties.length = 960;
        clipDraft.properties.clipLen = 960;
        trackDraft.clips.append(clipDraft);
    }
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, trackDraft));
    appendEmptyTrack(runtime, commandContext(), QStringLiteral("Anchor target"));

    QList<Clip *> clips;
    for (auto *clip : appModel->tracks().at(0)->clips())
        clips.append(clip);
    QCOMPARE(clips.size(), 3);
    const auto firstId = clips.at(0)->id();
    const auto lastId = clips.at(2)->id();

    editor.resize(editorWidth, 560);
    canvas->setAnimationEnabled(false);
    editor.show();
    editor.activateWindow();
    canvas->setFocus();
    QTRY_VERIFY(editor.isVisible() && canvas->viewport()->isVisible());
    // Native window systems may constrain the requested window size to the screen.
    // Fit the fixture's clips to the actual canvas instead of requiring a desktop width.
    constexpr int visibleTicks = 5760;
    const auto availableWidth = canvas->viewport()->width() -
                                TracksEditorGlobal::trackViewLeftMargin - 20;
    QVERIFY(availableWidth > 0);
    const auto scale = availableWidth * static_cast<double>(AppGlobal::ticksPerQuarterNote) /
                       (visibleTicks * TracksEditorGlobal::pixelsPerQuarterNote);
    QVERIFY(canvas->setViewportScale(std::min(1.5, scale), 1.0));
    canvas->setViewportStartTick(0);
    QCoreApplication::processEvents();
    historyManager->reset();
    const auto before = runtime.documentVersion();

    auto *firstItem = editor.findClipItemById(firstId);
    auto *lastItem = editor.findClipItemById(lastId);
    QVERIFY(firstItem && lastItem);
    const auto pointFor = [canvas](const AbstractClipView *item) {
        return canvas->mapFromScene(item->sceneBoundingRect().center());
    };
    QVERIFY(canvas->viewport()->rect().contains(pointFor(firstItem)));
    QVERIFY(canvas->viewport()->rect().contains(pointFor(lastItem)));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, pointFor(firstItem));
    QCOMPARE(canonicalClipIds(appStatus->selectedClips.get()), canonicalClipIds({firstId}));

    // Clicking the empty lane below the clips clears the selection. The range anchor has to go with
    // it, so a Shift click afterwards selects one clip instead of ranging from the old one.
    const auto blank = canvas->mapFromScene(
        QPointF(canvas->sceneXForTick(480), TracksEditorGlobal::trackHeight * 1.5));
    QVERIFY(canvas->viewport()->rect().contains(blank));
    QVERIFY(!dynamic_cast<AbstractClipView *>(canvas->itemAt(blank)));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, blank);
    QVERIFY(appStatus->selectedClips.get().isEmpty());

    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::ShiftModifier, pointFor(lastItem));
    QCOMPARE(canonicalClipIds(appStatus->selectedClips.get()), canonicalClipIds({lastId}));

    // A right press keeps the group whatever modifiers come with it: the menu decides what to act
    // on, so Ctrl must not toggle the clip away and Shift must not rebuild the range.
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::ControlModifier, pointFor(firstItem));
    QCOMPARE(canonicalClipIds(appStatus->selectedClips.get()),
             canonicalClipIds({firstId, lastId}));
    auto *secondItem = editor.findClipItemById(clips.at(1)->id());
    QVERIFY(secondItem);
    for (const auto modifier : {Qt::ControlModifier, Qt::ShiftModifier}) {
        QTest::mouseClick(canvas->viewport(), Qt::RightButton, modifier, pointFor(firstItem));
        QCOMPARE(canonicalClipIds(appStatus->selectedClips.get()),
                 canonicalClipIds({firstId, lastId}));
    }
    QTest::mouseClick(canvas->viewport(), Qt::RightButton, Qt::ControlModifier,
                      pointFor(secondItem));
    QCOMPARE(canonicalClipIds(appStatus->selectedClips.get()), canonicalClipIds({clips.at(1)->id()}));
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}
