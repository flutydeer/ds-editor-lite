#include "tst_application_gui.h"
#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/TrackController.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/TrackEditor/GraphicsItem/AbstractClipView.h"
#include "UI/Views/TrackEditor/TrackEditorView.h"
#include "UI/Views/TrackEditor/TracksGraphicsView.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QApplication>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QtTest/QTest>

void ApplicationGuiTests::trackClipDragCommitsOrCancels_data() {
    QTest::addColumn<bool>("cancel");
    QTest::newRow("release-commits") << false;
    QTest::newRow("escape-cancels") << true;
}

void ApplicationGuiTests::trackClipDragCommitsOrCancels() {
    QFETCH(bool, cancel);
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));

    const auto clearDialogParent = qScopeGuard([] { trackController->setParentWidget(nullptr); });
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
    Automation::TrackDraftDto trackDraft;
    trackDraft.name = QStringLiteral("Track gesture");
    trackDraft.defaultLanguage = QStringLiteral("eng");
    trackDraft.clips.append(clipDraft);
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, trackDraft));
    QCOMPARE(context->m_appModel->tracks().size(), 1);
    auto *track = context->m_appModel->tracks().first();
    QCOMPARE(track->clips().count(), 1);
    const auto *clip = dynamic_cast<SingingClip *>(*track->clips().begin());
    QVERIFY(clip);
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
    const auto press = canvas->mapFromScene(originalRect.center());
    const auto deltaPixels = canvas->sceneXForTick(moveTicks) - canvas->sceneXForTick(0);
    const auto release = canvas->mapFromScene(originalRect.center() + QPointF(deltaPixels, 0));
    QVERIFY(canvas->viewport()->rect().contains(press));
    QVERIFY(canvas->viewport()->rect().contains(release));
    QCOMPARE(canvas->itemAt(press), static_cast<QGraphicsItem *>(item));
    QVERIFY(!item->isSelected());

    QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, press);
    QVERIFY(item->isSelected());
    QVERIFY(item->activeClip());
    QCOMPARE(canvas->selectedClipsId(), QList<int>{clipId});
    QCOMPARE(appStatus->selectedClips.get(), QList<int>{clipId});
    QCOMPARE(appStatus->activeClipId.get(), clipId);
    QCOMPARE(runtime.documentVersion(), before);

    QMouseEvent move(QEvent::MouseMove, QPointF(release),
                     QPointF(canvas->viewport()->mapToGlobal(release)), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas->viewport(), &move);
    QTRY_COMPARE(item->start(), originalStart + moveTicks);
    QCOMPARE(clip->start(), originalStart);
    QCOMPARE(clip->length(), clipLength);
    QCOMPARE(item->sceneBoundingRect().size(), originalRect.size());
    QCOMPARE(item->sceneBoundingRect().left(), originalRect.left() + deltaPixels);
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
    QCOMPARE(clip->start(), cancel ? originalStart : originalStart + moveTicks);
    QCOMPARE(item->start(), clip->start());
    QCOMPARE(clip->length(), clipLength);
    QCOMPARE(item->length(), clipLength);
    QCOMPARE(canvas->selectedClipsId(), QList<int>{clipId});
    QCOMPARE(appStatus->selectedClips.get(), QList<int>{clipId});

    if (cancel) {
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(item->sceneBoundingRect(), originalRect);
        QVERIFY(!historyManager->canUndo());
        QVERIFY(!historyManager->canRedo());
        return;
    }

    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QVERIFY(historyManager->canUndo());
    QVERIFY(!historyManager->canRedo());
    const auto undone = runtime.history().undo(commandContext());
    QVERIFY(undone && undone.get().changed);
    QCOMPARE(clip->start(), originalStart);
    item = editor.findClipItemById(clipId);
    QVERIFY(item);
    QCOMPARE(item->start(), originalStart);
    QCOMPARE(item->sceneBoundingRect(), originalRect);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 2);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(historyManager->canRedo());
}
