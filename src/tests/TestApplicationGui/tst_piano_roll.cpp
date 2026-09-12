#include "tst_application_gui.h"
#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/ClipController.h"
#include "Controller/ClipboardController.h"
#include "Controller/PlaybackController.h"
#include "Controller/TrackController.h"
#include "Global/ControllerGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/ClipEditor/PianoRoll/NoteView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollCoord.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsScene.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include "UI/Views/ClipEditor/PianoRoll/PronunciationView.h"
#include "UI/Views/ClipEditor/PianoRoll/SplitLineIndicator.h"
#include "UI/Views/ClipEditor/ToolBar/ClipEditorToolBarView.h"
#include "UI/Window/MainWindow.h"

#include <lite/GUI/Controls/InlineTextEditOverlay.h>
#include <lite/GUI/Controls/ToolTip.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QtTest/QTest>
#include <QApplication>
#include <QClipboard>
#include <QMouseEvent>
#include <QLineEdit>
#include <QMimeData>
#include <QScopeGuard>
#include <QAbstractButton>
#include <QTextDocument>

namespace {
    void replaceInlineText(QLineEdit *editor, const QString &text) {
        QVERIFY(editor);
        QTRY_VERIFY(editor->isVisible() && editor->hasFocus());
        QTest::keySequence(editor, QKeySequence::SelectAll);
        QTest::keyClicks(editor, text);
        QCOMPARE(editor->text(), text);
    }
}

void ApplicationGuiTests::createPianoRoll() {
    auto &runtime = *context->m_coreRuntime;
    Automation::TrackDraftDto track;
    track.name = QStringLiteral("Mouse gesture");
    track.defaultLanguage = QStringLiteral("eng");
    const auto insertedTrack = runtime.project().insertTrack(commandContext(), 0, track);
    QVERIFY(insertedTrack);
    QCOMPARE(insertedTrack.get().affectedObjects.size(), 1);
    trackId = Automation::TrackId(insertedTrack.get().affectedObjects.first().value);

    Automation::ClipDraftDto clip;
    clip.type = Automation::ClipDraftDto::Type::Singing;
    clip.properties.name = QStringLiteral("Mouse gesture");
    clip.properties.length = 3840;
    clip.properties.clipLen = 3840;
    clip.defaultLanguage = QStringLiteral("eng");
    const auto insertedClip =
        runtime.project().insertClips(commandContext(), {
                                                            {.trackId = trackId, .clip = clip}
    });
    QVERIFY(insertedClip);
    QCOMPARE(insertedClip.get().affectedObjects.size(), 1);
    singingClip = dynamic_cast<SingingClip *>(
        context->m_appModel->findClipById(insertedClip.get().affectedObjects.first().value));
    QVERIFY(singingClip);
    clipController->setClip(singingClip);
    appStatus->activeClipId = singingClip->id();
    appStatus->pianoRollQuantize = 16;
    appStatus->pianoRollQuantizeEnabled = true;

    scene = std::make_unique<PianoRollGraphicsScene>();
    view = std::make_unique<PianoRollGraphicsView>(scene.get());
    view->resize(900, 500);
    view->setDataContext(singingClip);
    view->setEditMode(ClipEditorGlobal::DrawNote);
    view->setAnimationEnabled(false);
    view->show();
    view->activateWindow();
    view->setFocus();
    QTRY_VERIFY(view->isVisible() && view->viewport()->width() > 800);
    view->setViewportScale(1.0, 1.0);
    view->setViewportCenterAt(1920, 60, false);
    QCoreApplication::processEvents();
    historyManager->reset();
}

void ApplicationGuiTests::selectionToolsDeleteOnlyTheChosenTimeAndKeyRange_data() {
    QTest::addColumn<bool>("backward");
    QTest::addColumn<bool>("rectangle");
    QTest::newRow("interval-forward") << false << false;
    QTest::newRow("interval-backward") << true << false;
    QTest::newRow("rectangle") << false << true;
}

void ApplicationGuiTests::selectionToolsDeleteOnlyTheChosenTimeAndKeyRange() {
    QFETCH(bool, backward);
    QFETCH(bool, rectangle);
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    QTRY_VERIFY(window.isActiveWindow());
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    QList<Automation::NoteDraftDto> drafts;
    for (const auto [start, key] : {
             QPair{480,  60},
             QPair{960,  72},
             QPair{1920, 65}
    }) {
        Automation::NoteDraftDto draft;
        draft.localStart = start;
        draft.length = 240;
        draft.keyIndex = key;
        draft.lyric = QStringLiteral("note-%1").arg(start);
        draft.language = QStringLiteral("eng");
        drafts.append(draft);
    }
    QVERIFY(runtime.notes().insertNotes(commandContext(), Automation::ClipId(singingClip->id()),
                                        drafts));
    const auto notes = singingClip->notes().toList();
    QCOMPARE(notes.size(), 3);
    const auto preserved = notes.last()->serialize();
    const auto outsideKeyRange = notes.at(1)->serialize();
    view->hide();
    window.activateWindow();
    QTRY_VERIFY(window.isActiveWindow());
    QVERIFY(window.showBottomPanelPage(QStringLiteral("ClipEditor")));
    QVERIFY(window.setPianoRollScale(1.0, 1.0));
    QVERIFY(window.centerPianoRollAt(1920, rectangle ? 61 : 66));
    auto *canvas = window.findChild<PianoRollGraphicsView *>();
    auto *toolbar = window.findChild<ClipEditorToolBarView *>();
    QVERIFY(canvas);
    QVERIFY(toolbar);
    auto *tool = toolbar->findChild<QAbstractButton *>(rectangle ? QStringLiteral("btnArrow")
                                                                 : QStringLiteral("btnBeam"));
    QVERIFY(tool);
    QTRY_VERIFY(tool->isVisible());
    QTest::mouseClick(tool, Qt::LeftButton);
    QCOMPARE(toolbar->editMode(),
             rectangle ? ClipEditorGlobal::Select : ClipEditorGlobal::IntervalSelect);
    canvas->setViewportStartTick(0);
    const auto pointAt = [&](const int tick, const int key) {
        return canvas->mapFromScene(QPointF(
            canvas->tickToSceneX(tick), PianoRollCoord::keyIndexToCenterY(
                                            key, ClipEditorGlobal::noteHeight * canvas->scaleY())));
    };
    const auto first = pointAt(360, rectangle ? 62 : 66);
    const auto last = pointAt(1320, rectangle ? 59 : 66);
    QVERIFY(canvas->viewport()->rect().contains(first));
    QVERIFY(canvas->viewport()->rect().contains(last));
    const auto press = backward ? last : first;
    const auto release = backward ? first : last;
    historyManager->reset();
    const auto before = runtime.documentVersion();
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, press);
    QMouseEvent move(QEvent::MouseMove, QPointF(release),
                     QPointF(canvas->viewport()->mapToGlobal(release)), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas->viewport(), &move);
    const auto selectingImage = canvas->viewport()->grab().toImage();
    QVERIFY(!selectingImage.isNull());
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, release);
    QVERIFY(canvas->viewport()->grab().toImage() != selectingImage);
    const auto selected = canvas->selectedNotesId();
    const auto expectedSelection =
        rectangle ? QSet<int>{notes.at(0)->id()} : QSet<int>{notes.at(0)->id(), notes.at(1)->id()};
    QCOMPARE(QSet<int>(selected.cbegin(), selected.cend()), expectedSelection);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(window.focusEditorRegion(EditorViewGlobal::Region::PianoRoll));
    QTest::keySequence(canvas->viewport(), QKeySequence::Delete);
    QTRY_COMPARE(singingClip->notes().count(), rectangle ? 2 : 1);
    QCOMPARE(singingClip->notes().toList().last()->serialize(), preserved);
    if (rectangle) {
        const auto *unselected = singingClip->findNoteById(notes.at(1)->id());
        QVERIFY(unselected);
        QCOMPARE(unselected->serialize(), outsideKeyRange);
    }
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(singingClip->notes().count(), 3);
    QCOMPARE(singingClip->notes().toList(), notes);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::copyPasteUsesTheActiveClipAndPlaybackPosition() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    const auto sourceId = insertSelectedNote();
    QVERIFY(sourceId >= 0);
    Automation::ClipDraftDto clip;
    clip.type = Automation::ClipDraftDto::Type::Singing;
    clip.properties.length = 3840;
    clip.properties.clipLen = 3840;
    clip.defaultLanguage = QStringLiteral("eng");
    const auto inserted =
        runtime.project().insertClips(commandContext(), {
                                                            {.trackId = trackId, .clip = clip}
    });
    QVERIFY(inserted);
    auto *target = dynamic_cast<SingingClip *>(
        context->m_appModel->findClipById(inserted.get().affectedObjects.first().value));
    QVERIFY(target);
    historyManager->reset();
    const auto beforeCopy = runtime.documentVersion();
    clipboardController->copy();
    QVERIFY(QApplication::clipboard()->mimeData()->hasFormat(
        ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams)));
    QCOMPARE(runtime.documentVersion(), beforeCopy);
    QVERIFY(!historyManager->canUndo());

    appStatus->activeClipId = target->id();
    playbackController->setPosition(1200);
    const auto beforePaste = runtime.documentVersion();
    clipboardController->paste();
    QCOMPARE(target->notes().count(), 1);
    QCOMPARE(singingClip->notes().count(), 1);
    const auto *pasted = *target->notes().begin();
    QCOMPARE(pasted->localStart(), 1200);
    QCOMPARE(pasted->length(), 240);
    QCOMPARE(pasted->keyIndex(), 62);
    QCOMPARE(pasted->lyric(), QStringLiteral("hello"));
    QCOMPARE(runtime.documentVersion().revision, beforePaste.revision + 1);
    QVERIFY(historyManager->canUndo());
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(target->notes().count(), 0);
    QVERIFY(singingClip->findNoteById(sourceId));
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::cutCopiesThenRemovesSelectionAsOneUndoStep() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    const auto noteId = insertSelectedNote();
    QVERIFY(noteId >= 0);
    historyManager->reset();
    const auto before = runtime.documentVersion();
    clipboardController->cut();
    QCOMPARE(singingClip->notes().count(), 0);
    QVERIFY(appStatus->selectedNotes.get().isEmpty());
    QVERIFY(QApplication::clipboard()->mimeData()->hasFormat(
        ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams)));
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QVERIFY(runtime.history().undo(commandContext()));
    QVERIFY(singingClip->findNoteById(noteId));
    QCOMPARE(sceneNoteCount(noteId), 1);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::wholeClipClipboardUsesSelectedTrackAndPreservesCurves() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    const auto sourceNoteId = insertSelectedNote();
    QVERIFY(sourceNoteId >= 0);
    Automation::CurveDraftDto curve;
    curve.type = Automation::CurveDraftDto::Type::Draw;
    curve.localStart = 0;
    curve.step = 120;
    curve.values = {6200, 6250, 6150};
    QVERIFY(runtime.parameters().replaceParameter(commandContext(),
                                                  Automation::ClipId(singingClip->id()),
                                                  ParamInfo::Pitch, Param::Edited, {curve}));
    Automation::TrackDraftDto draft;
    draft.name = QStringLiteral("Clipboard destination");
    draft.defaultLanguage = QStringLiteral("eng");
    const auto inserted = runtime.project().insertTrack(
        commandContext(), context->m_appModel->tracks().size(), draft);
    QVERIFY(inserted);
    auto *targetTrack = context->m_appModel->tracks().last();
    QVERIFY(targetTrack->id() != trackId.value());
    trackController->setSelectedClips({singingClip->id()});
    historyManager->reset();
    const auto beforeCopy = runtime.documentVersion();
    trackController->copySelectedClips();
    QVERIFY(QApplication::clipboard()->mimeData()->hasFormat(
        ControllerGlobal::ElemMimeType.at(ControllerGlobal::Clip)));
    QCOMPARE(runtime.documentVersion(), beforeCopy);
    QVERIFY(!historyManager->canUndo());

    trackController->setSelectedTrackIndex(context->m_appModel->tracks().size() - 1);
    playbackController->setPosition(1200);
    const auto beforePaste = runtime.documentVersion();
    clipboardController->paste();
    QCOMPARE(targetTrack->clips().count(), 1);
    const auto *pasted = dynamic_cast<SingingClip *>(*targetTrack->clips().begin());
    QVERIFY(pasted);
    QCOMPARE(pasted->start(), 1200);
    QCOMPARE(pasted->notes().count(), 1);
    QCOMPARE((*pasted->notes().begin())->localStart(), 480);
    QCOMPARE((*pasted->notes().begin())->lyric(), QStringLiteral("hello"));
    const auto parameter = runtime.parameters().getParameter(runtime.documentVersion().documentId,
                                                             Automation::ClipId(pasted->id()),
                                                             ParamInfo::Pitch, Param::Edited);
    QVERIFY(parameter);
    QCOMPARE(parameter.get().curves.size(), 1);
    QCOMPARE(parameter.get().curves.first().localStart, curve.localStart);
    QCOMPARE(parameter.get().curves.first().step, curve.step);
    QCOMPARE(parameter.get().curves.first().values, curve.values);
    QCOMPARE(runtime.documentVersion().revision, beforePaste.revision + 1);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(targetTrack->clips().count(), 0);
    QVERIFY(singingClip->findNoteById(sourceNoteId));
    QCOMPARE(sceneNoteCount(sourceNoteId), 1);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::invalidClipboardDoesNotEdit_data() {
    QTest::addColumn<QString>("format");
    QTest::addColumn<QByteArray>("bytes");
    const auto notes = ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams);
    QTest::newRow("unrelated-text") << QStringLiteral("text/plain") << QByteArray("hello");
    QTest::newRow("malformed-note-json") << notes << QByteArray("{broken");
    QTest::newRow("empty-note-payload") << notes << QByteArray("{}");
}

void ApplicationGuiTests::invalidClipboardDoesNotEdit() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    QFETCH(QString, format);
    QFETCH(QByteArray, bytes);
    auto &runtime = *context->m_coreRuntime;
    auto mime = std::make_unique<QMimeData>();
    mime->setData(format, bytes);
    QApplication::clipboard()->setMimeData(mime.release());
    const auto before = runtime.documentVersion();
    clipboardController->paste();
    QCOMPARE(singingClip->notes().count(), 0);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    QCOMPARE(sceneNoteCount(-1), 0);
}

void ApplicationGuiTests::pianoErasingRestoresSceneItemsOnCancelAndUndo() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    const auto firstId = insertSelectedNote();
    QVERIFY(firstId >= 0);
    Automation::NoteDraftDto second;
    second.localStart = 960;
    second.length = 240;
    second.keyIndex = 64;
    second.lyric = QStringLiteral("world");
    second.language = QStringLiteral("eng");
    const auto inserted = runtime.notes().insertNotes(
        commandContext(), Automation::ClipId(singingClip->id()), {second});
    QVERIFY(inserted);
    const auto secondId = inserted.get().affectedObjects.first().value;
    historyManager->reset();
    const auto before = runtime.documentVersion();
    view->setEditMode(ClipEditorGlobal::EraseNote);
    const auto first = pointFor(600, 62);
    const auto last = pointFor(1080, 64);
    const auto release = qScopeGuard(
        [&] { QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, last); });
    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, first);
    QCOMPARE(sceneNoteCount(firstId), 0);
    QMouseEvent move(QEvent::MouseMove, QPointF(last), QPointF(view->viewport()->mapToGlobal(last)),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &move);
    QCOMPARE(sceneNoteCount(secondId), 0);
    QCOMPARE(singingClip->notes().count(), 2);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(editSessionManager->hasActiveTransaction());
    QTest::keyClick(view.get(), Qt::Key_Escape);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, last);
    QCOMPARE(sceneNoteCount(firstId), 1);
    QCOMPARE(sceneNoteCount(secondId), 1);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QVERIFY(appStatus->pianoRollNoteErasePreview.get().isEmpty());
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());

    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, first);
    QVERIFY(!singingClip->findNoteById(firstId));
    QVERIFY(singingClip->findNoteById(secondId));
    QCOMPARE(sceneNoteCount(firstId), 0);
    QCOMPARE(sceneNoteCount(secondId), 1);
    QVERIFY(appStatus->pianoRollNoteErasePreview.get().isEmpty());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QVERIFY(runtime.history().undo(commandContext()));
    QVERIFY(singingClip->findNoteById(firstId));
    QCOMPARE(sceneNoteCount(firstId), 1);
    QCOMPARE(sceneNoteCount(secondId), 1);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::pianoSplitIndicatorFollowsTheMouseAndMatchesTheEdit() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    const auto noteId = insertSelectedNote();
    QVERIFY(noteId >= 0);
    historyManager->reset();
    const auto before = runtime.documentVersion();
    view->setEditMode(ClipEditorGlobal::SplitNote);
    SplitLineIndicator *indicator = nullptr;
    for (auto *item : scene->items()) {
        if (auto *line = dynamic_cast<SplitLineIndicator *>(item))
            indicator = line;
    }
    QVERIFY(indicator);
    const auto hover = [&](const QPoint &position) {
        QMouseEvent move(QEvent::MouseMove, QPointF(position),
                         QPointF(view->viewport()->mapToGlobal(position)), Qt::NoButton,
                         Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(view->viewport(), &move);
    };
    const auto point = pointFor(610, 62);
    hover(point);
    QVERIFY(indicator->isVisible());
    QVERIFY(!indicator->path().isEmpty());
    QCOMPARE(indicator->lastNoteView(), sceneNote(noteId));
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(singingClip->notes().count(), 1);
    hover(pointFor(1200, 70));
    QVERIFY(!indicator->isVisible());
    hover(point);
    QVERIFY(indicator->isVisible());
    QVERIFY(!indicator->path().isEmpty());
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, point);
    QCOMPARE(singingClip->notes().count(), 2);
    const auto *original = singingClip->findNoteById(noteId);
    QVERIFY(original);
    QCOMPARE(original->localStart(), 480);
    QCOMPARE(original->length(), 120);
    const auto *continuation = singingClip->notes().toList().last();
    QVERIFY(continuation->id() != noteId);
    QCOMPARE(continuation->localStart(), 600);
    QCOMPARE(continuation->length(), 120);
    QCOMPARE(continuation->keyIndex(), original->keyIndex());
    QCOMPARE(continuation->lyric(), QStringLiteral("-"));
    const auto continuationId = continuation->id();
    QCOMPARE(sceneNoteCount(continuationId), 1);
    QVERIFY(!indicator->isVisible());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(singingClip->findNoteById(noteId)->length(), 240);
    QCOMPARE(sceneNoteCount(noteId), 1);
    QCOMPARE(sceneNoteCount(continuationId), 0);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::drawingCommitsOnceAndUndoRedoUpdatesTheScene() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    constexpr int startTick = 480;
    constexpr int endTick = 960;
    constexpr int key = 60;
    const auto press = pointFor(startTick + 30, key);
    const auto release = pointFor(endTick + 30, key);
    QVERIFY(view->viewport()->rect().contains(press));
    QVERIFY(view->viewport()->rect().contains(release));
    const auto before = runtime.documentVersion();
    QVERIFY(singingClip->notes().count() == 0);
    QVERIFY(!historyManager->canUndo());

    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, press);
    QTRY_COMPARE(appStatus->pianoRollNoteEditPreview.get().size(), 1);
    QVERIFY(singingClip->notes().count() == 0);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(runtime.documentVersion() == before);
    QVERIFY(editSessionManager->hasActiveTransaction());
    QCOMPARE(sceneNoteCount(-1), 1);

    QMouseEvent move(QEvent::MouseMove, QPointF(release),
                     QPointF(view->viewport()->mapToGlobal(release)), Qt::NoButton, Qt::LeftButton,
                     Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &move);
    const auto preview = appStatus->pianoRollNoteEditPreview.get();
    QCOMPARE(preview.size(), 1);
    QCOMPARE(preview.first().rStart, startTick);
    QCOMPARE(preview.first().length, endTick - startTick);
    QCOMPARE(preview.first().keyIndex, key);
    QVERIFY(singingClip->notes().count() == 0);
    QVERIFY(runtime.documentVersion() == before);

    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, release);
    QTRY_COMPARE(singingClip->notes().count(), 1);
    const auto *note = *singingClip->notes().begin();
    const auto noteId = note->id();
    QCOMPARE(note->localStart(), startTick);
    QCOMPARE(note->length(), endTick - startTick);
    QCOMPARE(note->keyIndex(), key);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QVERIFY(historyManager->canUndo());
    QVERIFY(!historyManager->canRedo());
    QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(appStatus->currentEditObject.get(), AppStatus::EditObjectType::None);
    QCOMPARE(view->selectedNotesId(), QList<int>{noteId});
    QCOMPARE(appStatus->selectedNotes.get(), QList<int>{noteId});
    QCOMPARE(sceneNoteCount(noteId), 1);
    QCOMPARE(sceneNoteCount(-1), 0);

    const auto undone = runtime.history().undo(commandContext());
    QVERIFY(undone && undone.get().changed);
    QTRY_VERIFY(singingClip->notes().count() == 0);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(historyManager->canRedo());
    QCOMPARE(sceneNoteCount(noteId), 0);
    QVERIFY(view->selectedNotesId().isEmpty());

    const auto redone = runtime.history().redo(commandContext());
    QVERIFY(redone && redone.get().changed);
    QTRY_COMPARE(singingClip->notes().count(), 1);
    const auto *restored = singingClip->findNoteById(noteId);
    QVERIFY(restored);
    QCOMPARE(restored->localStart(), startTick);
    QCOMPARE(restored->length(), endTick - startTick);
    QCOMPARE(restored->keyIndex(), key);
    QCOMPARE(sceneNoteCount(noteId), 1);
    QVERIFY(historyManager->canUndo());
    QVERIFY(!historyManager->canRedo());
}

void ApplicationGuiTests::draggingExistingNoteCommitsOrCancels_data() {
    QTest::addColumn<bool>("cancel");
    QTest::newRow("release-commits") << false;
    QTest::newRow("escape-cancels") << true;
}

void ApplicationGuiTests::draggingExistingNoteCommitsOrCancels() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    QFETCH(bool, cancel);
    auto &runtime = *context->m_coreRuntime;
    const auto noteId = insertSelectedNote();
    QVERIFY(noteId >= 0);
    view->setEditMode(ClipEditorGlobal::Select);
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto *item = sceneNote(noteId);
    QVERIFY(item);
    const auto originalPosition = item->scenePos();
    const auto press = pointFor(600, 62);
    const auto release = pointFor(1080, 64);
    QVERIFY(view->viewport()->rect().contains(press));
    QVERIFY(view->viewport()->rect().contains(release));
    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, press);
    QMouseEvent move(QEvent::MouseMove, QPointF(release),
                     QPointF(view->viewport()->mapToGlobal(release)), Qt::NoButton, Qt::LeftButton,
                     Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &move);
    QTRY_COMPARE(appStatus->pianoRollNoteEditPreview.get().size(), 1);
    const auto preview = appStatus->pianoRollNoteEditPreview.get().first();
    QCOMPARE(preview.rStart, 960);
    QCOMPARE(preview.length, 240);
    QCOMPARE(preview.keyIndex, 64);
    QCOMPARE(item->startOffset(), 480);
    QCOMPARE(item->keyOffset(), 2);
    QVERIFY(item->scenePos() != originalPosition);
    QCOMPARE(singingClip->findNoteById(noteId)->localStart(), 480);
    QCOMPARE(singingClip->findNoteById(noteId)->keyIndex(), 62);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(editSessionManager->hasActiveTransaction());

    if (cancel)
        QTest::keyClick(view.get(), Qt::Key_Escape);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, release);
    QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(appStatus->currentEditObject.get(), AppStatus::EditObjectType::None);
    QCOMPARE(sceneNoteCount(noteId), 1);
    item = sceneNote(noteId);
    QVERIFY(item);
    QCOMPARE(item->startOffset(), 0);
    QCOMPARE(item->keyOffset(), 0);
    const auto *note = singingClip->findNoteById(noteId);
    QVERIFY(note);
    QCOMPARE(note->localStart(), cancel ? 480 : 960);
    QCOMPARE(note->keyIndex(), cancel ? 62 : 64);
    QCOMPARE(item->rStart(), note->localStart());
    QCOMPARE(item->keyIndex(), note->keyIndex());
    if (cancel) {
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(item->scenePos(), originalPosition);
        QVERIFY(!historyManager->canUndo());
        return;
    }
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(singingClip->findNoteById(noteId)->localStart(), 480);
    QCOMPARE(singingClip->findNoteById(noteId)->keyIndex(), 62);
    QVERIFY(sceneNote(noteId));
    QCOMPARE(sceneNote(noteId)->scenePos(), originalPosition);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(runtime.history().redo(commandContext()));
    QCOMPARE(singingClip->findNoteById(noteId)->localStart(), 960);
    QCOMPARE(singingClip->findNoteById(noteId)->keyIndex(), 64);
    QVERIFY(sceneNote(noteId));
    QCOMPARE(sceneNote(noteId)->rStart(), 960);
    QCOMPARE(sceneNote(noteId)->keyIndex(), 64);
}

int ApplicationGuiTests::insertSelectedNote() {
    Automation::NoteDraftDto note;
    note.localStart = 480;
    note.length = 240;
    note.keyIndex = 62;
    note.lyric = QStringLiteral("hello");
    note.language = QStringLiteral("eng");
    const auto inserted = context->m_coreRuntime->notes().insertNotes(
        commandContext(), Automation::ClipId(singingClip->id()), {note});
    if (!inserted || inserted.get().affectedObjects.isEmpty())
        return -1;
    const auto id = inserted.get().affectedObjects.first().value;
    appStatus->selectedNotes = QList<int>{id};
    return id;
}

void ApplicationGuiTests::inlineLyricsCommitNavigateAndCancel() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    QList<Automation::NoteDraftDto> drafts;
    for (int index = 0; index < 2; ++index) {
        Automation::NoteDraftDto note;
        note.localStart = 480 + index * 480;
        note.length = 480;
        note.keyIndex = 62;
        note.lyric = index == 0 ? QStringLiteral("one") : QStringLiteral("two");
        note.language = QStringLiteral("eng");
        drafts.append(note);
    }
    QVERIFY(runtime.notes().insertNotes(commandContext(), Automation::ClipId(singingClip->id()),
                                        drafts));
    const auto notes = singingClip->notes().toList();
    QCOMPARE(notes.size(), 2);
    auto *first = notes.first();
    auto *second = notes.last();
    view->setEditMode(ClipEditorGlobal::Select);
    auto *overlay = view->findChild<InlineTextEditOverlay *>();
    QVERIFY(overlay);
    const auto cancelOnFailure = qScopeGuard([&] {
        if (overlay->isEditing())
            QTest::keyClick(overlay->findChild<QLineEdit *>(), Qt::Key_Escape);
    });
    const auto editFirst = [&] {
        const auto position = pointFor(720, 62);
        QTest::mouseDClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, position);
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, position);
        QTRY_VERIFY(overlay->isEditing());
        QVERIFY(sceneNote(first->id())->isEditingLyric());
    };
    historyManager->reset();
    editFirst();
    if (QTest::currentTestFailed())
        return;
    auto *input = overlay->findChild<QLineEdit *>();
    const auto replacement = QStringLiteral("a longer replacement");
    replaceInlineText(input, QStringLiteral("  ") + replacement + QStringLiteral("  "));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(first->lyric(), QStringLiteral("one"));
    QVERIFY(!historyManager->canUndo());
    QTest::keyClick(input, Qt::Key_Return);
    QVERIFY(!overlay->isEditing());
    QCOMPARE(first->lyric(), replacement);
    QCOMPARE(sceneNote(first->id())->lyric(), first->lyric());
    QVERIFY(!sceneNote(first->id())->isEditingLyric());
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(first->lyric(), QStringLiteral("one"));
    QVERIFY(!historyManager->canUndo());
    QVERIFY(runtime.history().redo(commandContext()));

    const auto beforeHover = runtime.documentVersion();
    const auto *beforeHoverUndo = historyManager->nextUndoEntry();
    QVERIFY(view->setViewportScale(0.5, 2.0));
    view->setViewportCenterAt(1920, 62, false);
    QTRY_VERIFY(sceneNote(first->id())->isLyricElided(view->visibleRect()));
    auto *tooltip = view->viewport()->findChild<ToolTip *>();
    QVERIFY(tooltip);
    QTest::mouseMove(view->viewport(), pointFor(720, 62));
    QTRY_VERIFY(tooltip->isVisible());
    QTextDocument tooltipText;
    tooltipText.setHtml(tooltip->title());
    QCOMPARE(tooltipText.toPlainText(), first->lyric());
    QTest::mouseMove(view->viewport(), pointFor(1800, 64));
    QTRY_VERIFY(!tooltip->isVisible());
    QCOMPARE(runtime.documentVersion(), beforeHover);
    QCOMPARE(historyManager->nextUndoEntry(), beforeHoverUndo);
    QVERIFY(view->setViewportScale(1.0, 1.0));
    view->setViewportCenterAt(1920, 60, false);

    editFirst();
    if (QTest::currentTestFailed())
        return;
    replaceInlineText(input, QStringLiteral("first tab"));
    if (QTest::currentTestFailed())
        return;
    QTest::keyClick(input, Qt::Key_Tab);
    QCOMPARE(first->lyric(), QStringLiteral("first tab"));
    QTRY_VERIFY(overlay->isEditing() && input->hasFocus());
    QCOMPARE(input->text(), QStringLiteral("two"));
    QCOMPARE(view->selectedNotesId(), QList<int>{second->id()});
    QVERIFY(sceneNote(second->id())->isEditingLyric());
    replaceInlineText(input, QStringLiteral("second tab"));
    if (QTest::currentTestFailed())
        return;
    QTest::keyClick(input, Qt::Key_Backtab);
    QCOMPARE(second->lyric(), QStringLiteral("second tab"));
    QTRY_VERIFY(overlay->isEditing() && input->hasFocus());
    QCOMPARE(input->text(), QStringLiteral("first tab"));
    QCOMPARE(view->selectedNotesId(), QList<int>{first->id()});
    const auto *beforeCancel = historyManager->nextUndoEntry();
    replaceInlineText(input, QStringLiteral("discarded"));
    if (QTest::currentTestFailed())
        return;
    QTest::keyClick(input, Qt::Key_Escape);
    QVERIFY(!overlay->isEditing());
    QCOMPARE(first->lyric(), QStringLiteral("first tab"));
    QCOMPARE(historyManager->nextUndoEntry(), beforeCancel);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(second->lyric(), QStringLiteral("two"));
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(first->lyric(), replacement);
}

void ApplicationGuiTests::inlinePronunciationCommitsAndCancels() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    Automation::NoteDraftDto draft;
    draft.localStart = 480;
    draft.length = 480;
    draft.keyIndex = 62;
    draft.lyric = QStringLiteral("la");
    draft.language = QStringLiteral("eng");
    draft.pronunciation.edited = QStringLiteral("l aa");
    QVERIFY(runtime.notes().insertNotes(commandContext(), Automation::ClipId(singingClip->id()),
                                        {draft}));
    auto *note = *singingClip->notes().begin();
    view->setEditMode(ClipEditorGlobal::Select);
    auto *overlay = view->findChild<InlineTextEditOverlay *>();
    QVERIFY(overlay);
    const auto cancelOnFailure = qScopeGuard([&] {
        if (overlay->isEditing())
            QTest::keyClick(overlay->findChild<QLineEdit *>(), Qt::Key_Escape);
    });
    const auto editPronunciation = [&] {
        const auto *pronunciation = sceneNote(note->id())->pronunciationView();
        QVERIFY(pronunciation->isVisible());
        const auto position = view->mapFromScene(pronunciation->sceneBoundingRect().center());
        QVERIFY(view->viewport()->rect().contains(position));
        QTest::mouseDClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, position);
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, position);
        QTRY_VERIFY(overlay->isEditing());
        QVERIFY(pronunciation->isEditingPronunciation());
    };
    historyManager->reset();
    editPronunciation();
    if (QTest::currentTestFailed())
        return;
    auto *input = overlay->findChild<QLineEdit *>();
    QCOMPARE(input->property("editRole").toString(), QStringLiteral("Pronunciation"));
    replaceInlineText(input, QStringLiteral("  m aa  "));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(note->pronunciation().edited, QStringLiteral("l aa"));
    QTest::keyClick(input, Qt::Key_Return);
    QVERIFY(!overlay->isEditing());
    QCOMPARE(note->pronunciation().edited, QStringLiteral("m aa"));
    QVERIFY(!sceneNote(note->id())->pronunciationView()->isEditingPronunciation());
    const auto *beforeCancel = historyManager->nextUndoEntry();
    QVERIFY(beforeCancel);
    editPronunciation();
    if (QTest::currentTestFailed())
        return;
    replaceInlineText(input, QStringLiteral("discarded"));
    if (QTest::currentTestFailed())
        return;
    QTest::keyClick(input, Qt::Key_Escape);
    QCOMPARE(note->pronunciation().edited, QStringLiteral("m aa"));
    QCOMPARE(historyManager->nextUndoEntry(), beforeCancel);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(note->pronunciation().edited, QStringLiteral("l aa"));
    QVERIFY(!historyManager->canUndo());
    QVERIFY(runtime.history().redo(commandContext()));
    QCOMPARE(note->pronunciation().edited, QStringLiteral("m aa"));
}

void ApplicationGuiTests::resizingANotePreviewsAndCommitsItsBoundary_data() {
    QTest::addColumn<bool>("leftEdge");
    QTest::addColumn<bool>("cancel");
    QTest::newRow("extend-right") << false << false;
    QTest::newRow("extend-left") << true << false;
    QTest::newRow("cancel-left") << true << true;
}

void ApplicationGuiTests::resizingANotePreviewsAndCommitsItsBoundary() {
    QFETCH(bool, leftEdge);
    QFETCH(bool, cancel);
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    const auto id = insertSelectedNote();
    QVERIFY(id >= 0);
    view->setEditMode(ClipEditorGlobal::Select);
    historyManager->reset();
    auto &runtime = *context->m_coreRuntime;
    const auto before = runtime.documentVersion();
    const auto *note = singingClip->findNoteById(id);
    const auto *item = sceneNote(id);
    QVERIFY(note);
    QVERIFY(item);
    const auto bounds = item->sceneBoundingRect();
    const auto press = view->mapFromScene(
        QPointF(leftEdge ? bounds.left() + 2 : bounds.right() - 2, bounds.center().y()));
    const auto delta = view->tickToSceneX(240) - view->tickToSceneX(0);
    const auto release = press + QPoint(qRound(leftEdge ? -delta : delta), 0);
    QVERIFY(view->viewport()->rect().contains(press));
    QVERIFY(view->viewport()->rect().contains(release));
    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, press);
    const auto releaseOnFailure = qScopeGuard([&] {
        if (editSessionManager->hasActiveTransaction()) {
            QTest::keyClick(view.get(), Qt::Key_Escape);
            QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, release);
        }
    });
    QMouseEvent move(QEvent::MouseMove, QPointF(release),
                     QPointF(view->viewport()->mapToGlobal(release)), Qt::NoButton, Qt::LeftButton,
                     Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &move);
    QTRY_COMPARE(appStatus->pianoRollNoteEditPreview.get().size(), 1);
    const auto preview = appStatus->pianoRollNoteEditPreview.get().first();
    const auto expectedStart = leftEdge ? 240 : 480;
    QCOMPARE(preview.rStart, expectedStart);
    QCOMPARE(preview.length, 480);
    QCOMPARE(preview.keyIndex, 62);
    QCOMPARE(note->length(), 240);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    if (cancel)
        QTest::keyClick(view.get(), Qt::Key_Escape);
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, release);
    if (cancel) {
        QCOMPARE(note->localStart(), 480);
        QCOMPARE(note->length(), 240);
        QCOMPARE(sceneNote(id)->length(), 240);
        QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
        QCOMPARE(runtime.documentVersion(), before);
        QVERIFY(!historyManager->canUndo());
        return;
    }
    QCOMPARE(note->localStart(), expectedStart);
    QCOMPARE(note->length(), 480);
    QCOMPARE(note->keyIndex(), 62);
    QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
    QCOMPARE(sceneNote(id)->length(), 480);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    historyManager->undo();
    QCOMPARE(note->localStart(), 480);
    QCOMPARE(note->length(), 240);
    QCOMPARE(sceneNote(id)->length(), 240);
    QVERIFY(!historyManager->canUndo());
    historyManager->redo();
    QCOMPARE(note->localStart(), expectedStart);
    QCOMPARE(note->length(), 480);
    QCOMPARE(sceneNote(id)->length(), 480);
}

QPoint ApplicationGuiTests::pointFor(int tick, int key) const {
    return view->mapFromScene(QPointF(
        view->tickToSceneX(tick),
        PianoRollCoord::keyIndexToCenterY(key, ClipEditorGlobal::noteHeight * view->scaleY())));
}

int ApplicationGuiTests::sceneNoteCount(int id) const {
    int count = 0;
    for (const auto *item : scene->items()) {
        const auto *note = dynamic_cast<const NoteView *>(item);
        if (note && note->id() == id)
            ++count;
    }
    return count;
}

const NoteView *ApplicationGuiTests::sceneNote(int id) const {
    for (const auto *item : scene->items()) {
        const auto *note = dynamic_cast<const NoteView *>(item);
        if (note && note->id() == id)
            return note;
    }
    return nullptr;
}
