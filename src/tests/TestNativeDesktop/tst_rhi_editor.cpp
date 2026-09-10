#include "tst_native_desktop.h"
#include "../TestSupport/GuiAppFixture.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Bootstrap/AppEnvironment.h"
#include "Controller/ClipController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/OutputSystem.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollRhiWidget.h"

#include <lite/GUI/Theme/ThemeIds.h>
#include <lite/GUI/Theme/ThemeLoader.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/History/ActionSequence.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <TalcsDevice/AudioDevice.h>

#include <QApplication>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

namespace {
    struct ExistingRhiNoteFixture {
        ~ExistingRhiNoteFixture() {
            if (canvas) {
                QTest::keyClick(canvas.get(), Qt::Key_Escape);
                QTest::mouseRelease(canvas.get(), Qt::LeftButton, Qt::NoModifier,
                                    canvas->rect().center());
                canvas->setDataContext(nullptr);
                canvas.reset();
                clipController->setClip(nullptr);
            }
        }

        Automation::CoreRuntime &runtime() const {
            return *app.context->m_coreRuntime;
        }

        Automation::CommandContext command() const {
            return {.expected = runtime().documentVersion(),
                    .source = Automation::InvocationSource::Test};
        }

        void initialize() {
            QVERIFY2(app.initialize(), qPrintable(app.error));
            Automation::NoteDraftDto note;
            note.localStart = 480;
            note.length = 480;
            note.keyIndex = 60;
            note.lyric = QStringLiteral("la");
            note.language = QStringLiteral("eng");
            Automation::ClipDraftDto draft;
            draft.type = Automation::ClipDraftDto::Type::Singing;
            draft.properties.length = 3840;
            draft.properties.clipLen = 3840;
            draft.defaultLanguage = QStringLiteral("eng");
            draft.notes.append(note);
            Automation::TrackDraftDto track;
            track.name = QStringLiteral("RHI existing note");
            track.clips.append(draft);
            QVERIFY(runtime().project().insertTrack(command(), 0, track));
            clip = dynamic_cast<SingingClip *>(
                *app.context->m_appModel->tracks().first()->clips().begin());
            QVERIFY(clip);
            QCOMPARE(clip->notes().count(), 1);
            noteId = (*clip->notes().begin())->id();
            clipController->setClip(clip);
            appStatus->activeClipId = clip->id();
            appStatus->pianoRollQuantize = 16;
            appStatus->pianoRollQuantizeEnabled = true;
            canvas = std::make_unique<PianoRollRhiWidget>();
            canvas->setApi(QRhiWidget::Api::Null);
            canvas->setDataContext(clip);
            canvas->setEditMode(ClipEditorGlobal::Select);
            QObject::connect(canvas.get(), &EditorRhiWidget::backendFailed, canvas.get(),
                             [this](const QString &reason) { backendError = reason; });
            submitted = std::make_unique<QSignalSpy>(canvas.get(), &QRhiWidget::frameSubmitted);
            canvas->resize(900, 500);
            canvas->show();
            canvas->activateWindow();
            canvas->setFocus();
            frameAfter(0);
            if (QTest::currentTestFailed())
                return;
            QVERIFY(canvas->setViewScale(1, 1));
            QVERIFY(canvas->centerAt(1920, 60));
            QTRY_VERIFY(canvas->startTick() < 480 && canvas->endTick() > 1440);
            historyManager->reset();
        }

        QPoint pointFor(double tick, double key) const {
            return {qRound((tick - canvas->startTick()) * canvas->width() /
                           (canvas->endTick() - canvas->startTick())),
                    qRound(canvas->height() / 2.0 + (canvas->centerKeyIndex() - key) *
                                                        ClipEditorGlobal::noteHeight *
                                                        canvas->scaleY())};
        }

        void moveTo(const QPoint &position) const {
            QMouseEvent move(QEvent::MouseMove, QPointF(position),
                             QPointF(canvas->mapToGlobal(position)), Qt::NoButton, Qt::LeftButton,
                             Qt::NoModifier);
            QApplication::sendEvent(canvas.get(), &move);
        }

        void frameAfter(qsizetype count) const {
            QTRY_VERIFY(submitted->size() > count || !backendError.isEmpty());
            QVERIFY2(backendError.isEmpty(), qPrintable(backendError));
        }

        GuiAppFixture app;
        SingingClip *clip = nullptr;
        int noteId = -1;
        std::unique_ptr<PianoRollRhiWidget> canvas;
        std::unique_ptr<QSignalSpy> submitted;
        QString backendError;
    };
}

void NativeDesktopTests::rhiNoteDrawingCommitsAndUndoUpdatesInteraction() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    GuiAppFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.error));
    auto &context = *fixture.context;
    auto &runtime = *context.m_coreRuntime;
    const auto command = [&] {
        return Automation::CommandContext{.expected = runtime.documentVersion(),
                                          .source = Automation::InvocationSource::Test};
    };
    Automation::ClipDraftDto clipDraft;
    clipDraft.type = Automation::ClipDraftDto::Type::Singing;
    clipDraft.properties.length = 3840;
    clipDraft.properties.clipLen = 3840;
    clipDraft.defaultLanguage = QStringLiteral("eng");
    Automation::TrackDraftDto trackDraft;
    trackDraft.name = QStringLiteral("RHI interaction");
    trackDraft.clips.append(clipDraft);
    QVERIFY(runtime.project().insertTrack(command(), 0, trackDraft));
    auto *track = context.m_appModel->tracks().first();
    QCOMPARE(track->clips().count(), 1);
    auto *clip = dynamic_cast<SingingClip *>(*track->clips().begin());
    QVERIFY(clip);
    clipController->setClip(clip);
    appStatus->activeClipId = clip->id();
    appStatus->pianoRollQuantize = 16;
    appStatus->pianoRollQuantizeEnabled = true;
    historyManager->reset();

    QString backendError;
    PianoRollRhiWidget canvas;
    // Null exercises the real RHI widget and command path without asserting GPU pixels.
    canvas.setApi(QRhiWidget::Api::Null);
    canvas.setDataContext(clip);
    canvas.setEditMode(ClipEditorGlobal::DrawNote);
    const auto detach = qScopeGuard([&] {
        canvas.setDataContext(nullptr);
        clipController->setClip(nullptr);
    });
    connect(&canvas, &EditorRhiWidget::backendFailed, &canvas,
            [&](const QString &reason) { backendError = reason; });
    QSignalSpy submitted(&canvas, &QRhiWidget::frameSubmitted);
    QSignalSpy renderFailed(&canvas, &QRhiWidget::renderFailed);
    canvas.resize(900, 500);
    canvas.show();
    canvas.activateWindow();
    canvas.setFocus();
    QTRY_VERIFY(!submitted.isEmpty() || !backendError.isEmpty());
    QVERIFY2(backendError.isEmpty(), qPrintable(backendError));
    QVERIFY(renderFailed.isEmpty());
    QCOMPARE(canvas.api(), QRhiWidget::Api::Null);
    QVERIFY(canvas.setViewScale(1.0, 1.0));
    QVERIFY(canvas.centerAt(1920, 60));
    QTRY_VERIFY(canvas.startTick() < 480 && canvas.endTick() > 1080);
    const auto pointForTick = [&](const double tick) {
        return QPoint(qRound((tick - canvas.startTick()) * canvas.width() /
                             (canvas.endTick() - canvas.startTick())),
                      canvas.height() / 2);
    };
    const auto press = pointForTick(540);
    const auto release = pointForTick(1020);
    QVERIFY(canvas.rect().contains(press));
    QVERIFY(canvas.rect().contains(release));
    const auto before = runtime.documentVersion();
    const auto beforePreviewFrame = submitted.size();
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, press);
    QMouseEvent move(QEvent::MouseMove, QPointF(release), QPointF(canvas.mapToGlobal(release)),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &move);
    QTRY_COMPARE(appStatus->pianoRollNoteEditPreview.get().size(), 1);
    const auto preview = appStatus->pianoRollNoteEditPreview.get().first();
    QCOMPARE(preview.rStart, 480);
    QCOMPARE(preview.length, 480);
    QCOMPARE(preview.keyIndex, 60);
    QCOMPARE(clip->notes().count(), 0);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(editSessionManager->hasActiveTransaction());
    QTRY_VERIFY(submitted.size() > beforePreviewFrame || !backendError.isEmpty());
    QVERIFY2(backendError.isEmpty(), qPrintable(backendError));

    const auto beforeCommitFrame = submitted.size();
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, release);
    QCOMPARE(clip->notes().count(), 1);
    const auto *note = *clip->notes().begin();
    const auto noteId = note->id();
    QCOMPARE(note->localStart(), 480);
    QCOMPARE(note->length(), 480);
    QCOMPARE(note->keyIndex(), 60);
    QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    const auto *entry = historyManager->nextUndoEntry();
    QVERIFY(entry);
    QVERIFY(entry->focusTransition());
    const auto focus = entry->focusTransition()->after;
    QCOMPARE(canvas.focusVisibility(focus), HistoryFocusVisibility::Visible);
    QTRY_VERIFY(submitted.size() > beforeCommitFrame || !backendError.isEmpty());
    QVERIFY2(backendError.isEmpty(), qPrintable(backendError));
    canvas.setEditMode(ClipEditorGlobal::Select);
    QVERIFY(runtime.windowId());
    const Automation::GuiDocumentCommandContext selectionContext{
        .documentId = runtime.documentVersion().documentId,
        .expectedRevision = runtime.documentVersion().revision,
        .windowId = *runtime.windowId(),
        .source = Automation::InvocationSource::Test};
    QVERIFY(
        runtime.facade().setSelectedNotes(selectionContext, Automation::ClipId(clip->id()), {}));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, pointForTick(720));
    QCOMPARE(appStatus->selectedNotes.get(), QList<int>{noteId});

    const auto beforeUndoFrame = submitted.size();
    QVERIFY(runtime.history().undo(command()));
    QCOMPARE(clip->notes().count(), 0);
    QVERIFY(!historyManager->canUndo());
    QTRY_VERIFY(submitted.size() > beforeUndoFrame || !backendError.isEmpty());
    QVERIFY2(backendError.isEmpty(), qPrintable(backendError));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, pointForTick(720));
    QVERIFY(appStatus->selectedNotes.get().isEmpty());

    const auto beforeRedoFrame = submitted.size();
    QVERIFY(runtime.history().redo(command()));
    QVERIFY(clip->findNoteById(noteId));
    QCOMPARE(canvas.focusVisibility(focus), HistoryFocusVisibility::Visible);
    QTRY_VERIFY(submitted.size() > beforeRedoFrame || !backendError.isEmpty());
    QVERIFY2(backendError.isEmpty(), qPrintable(backendError));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, pointForTick(720));
    QCOMPARE(appStatus->selectedNotes.get(), QList<int>{noteId});
    QCOMPARE(runtime.documentVersion().revision, before.revision + 3);
    QVERIFY(renderFailed.isEmpty());
}

void NativeDesktopTests::rhiNoteMoveCanBeCanceledAndThenCommitted() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    ExistingRhiNoteFixture fixture;
    fixture.initialize();
    if (QTest::currentTestFailed())
        return;
    auto &canvas = *fixture.canvas;
    auto *note = fixture.clip->findNoteById(fixture.noteId);
    QVERIFY(note);
    const auto press = fixture.pointFor(720, 60);
    const auto release = fixture.pointFor(1200, 62);
    QVERIFY(canvas.rect().contains(press));
    QVERIFY(canvas.rect().contains(release));
    const auto before = fixture.runtime().documentVersion();
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, press);
    fixture.moveTo(release);
    QTRY_COMPARE(appStatus->pianoRollNoteEditPreview.get().size(), 1);
    const auto preview = appStatus->pianoRollNoteEditPreview.get().first();
    QCOMPARE(preview.rStart, 960);
    QCOMPARE(preview.keyIndex, 62);
    QCOMPARE(note->localStart(), 480);
    QCOMPARE(note->keyIndex(), 60);
    QVERIFY(editSessionManager->hasActiveTransaction());
    QCOMPARE(fixture.runtime().documentVersion(), before);
    const auto canceledFrame = fixture.submitted->size();
    QTest::keyClick(&canvas, Qt::Key_Escape);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, release);
    QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(note->localStart(), 480);
    QCOMPARE(note->keyIndex(), 60);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    fixture.frameAfter(canceledFrame);
    if (QTest::currentTestFailed())
        return;
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, press);
    fixture.moveTo(release);
    const auto committedFrame = fixture.submitted->size();
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, release);
    QCOMPARE(note->localStart(), 960);
    QCOMPARE(note->keyIndex(), 62);
    QCOMPARE(note->length(), 480);
    QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(fixture.runtime().documentVersion().revision, before.revision + 1);
    fixture.frameAfter(committedFrame);
    if (QTest::currentTestFailed())
        return;
    const auto undoFrame = fixture.submitted->size();
    QVERIFY(fixture.runtime().history().undo(fixture.command()));
    QCOMPARE(note->localStart(), 480);
    QCOMPARE(note->keyIndex(), 60);
    QVERIFY(!historyManager->canUndo());
    fixture.frameAfter(undoFrame);
    if (QTest::currentTestFailed())
        return;
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, fixture.pointFor(1600, 70));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, press);
    QCOMPARE(appStatus->selectedNotes.get(), QList<int>{fixture.noteId});
}

void NativeDesktopTests::rhiNoteResizeUndoRestoresTheHitRegion() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    ExistingRhiNoteFixture fixture;
    fixture.initialize();
    if (QTest::currentTestFailed())
        return;
    auto &canvas = *fixture.canvas;
    auto *note = fixture.clip->findNoteById(fixture.noteId);
    QVERIFY(note);
    const auto press = fixture.pointFor(960, 60) - QPoint(2, 0);
    const auto release = fixture.pointFor(1200, 60) - QPoint(2, 0);
    QVERIFY(canvas.rect().contains(press));
    QVERIFY(canvas.rect().contains(release));
    const auto before = fixture.runtime().documentVersion();
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, press);
    fixture.moveTo(release);
    QTRY_COMPARE(appStatus->pianoRollNoteEditPreview.get().size(), 1);
    const auto preview = appStatus->pianoRollNoteEditPreview.get().first();
    QCOMPARE(preview.rStart, 480);
    QCOMPARE(preview.length, 720);
    QCOMPARE(note->length(), 480);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    const auto committedFrame = fixture.submitted->size();
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, release);
    QCOMPARE(note->length(), 720);
    QCOMPARE(fixture.runtime().documentVersion().revision, before.revision + 1);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    fixture.frameAfter(committedFrame);
    if (QTest::currentTestFailed())
        return;
    const auto extendedArea = fixture.pointFor(1080, 60);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, extendedArea);
    QCOMPARE(appStatus->selectedNotes.get(), QList<int>{fixture.noteId});
    const auto undoFrame = fixture.submitted->size();
    QVERIFY(fixture.runtime().history().undo(fixture.command()));
    QCOMPARE(note->length(), 480);
    fixture.frameAfter(undoFrame);
    if (QTest::currentTestFailed())
        return;
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, extendedArea);
    QVERIFY(appStatus->selectedNotes.get().isEmpty());
    QVERIFY(!historyManager->canUndo());
    QVERIFY(historyManager->canRedo());
}

void NativeDesktopTests::rhiPitchAnchorInsertionAndCanceledDragUseTheRealEditor() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    ExistingRhiNoteFixture fixture;
    fixture.initialize();
    if (QTest::currentTestFailed())
        return;
    auto &canvas = *fixture.canvas;
    Automation::CurveDraftDto draft;
    draft.type = Automation::CurveDraftDto::Type::Anchor;
    draft.nodes = {
        {480,  6000, AnchorNode::Hermite},
        {1440, 6000, AnchorNode::None   }
    };
    QVERIFY(fixture.runtime().parameters().replaceParameter(
        fixture.command(), Automation::ClipId(fixture.clip->id()), ParamInfo::Pitch, Param::Edited,
        {draft}));
    auto *pitch = fixture.clip->params.getParamByName(ParamInfo::Pitch);
    QVERIFY(pitch);
    const auto anchorCurve = [&] {
        return dynamic_cast<const AnchorCurve *>(pitch->curves(Param::Edited).value(0));
    };
    QVERIFY(anchorCurve());
    canvas.setEditMode(ClipEditorGlobal::EditPitchAnchor);
    historyManager->reset();
    const auto before = fixture.runtime().documentVersion();
    const auto position = fixture.pointFor(960, 62);
    QVERIFY(canvas.rect().contains(position));
    const auto insertedFrame = fixture.submitted->size();
    QTest::mouseDClick(&canvas, Qt::LeftButton, Qt::NoModifier, position);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, position);
    QVERIFY(anchorCurve());
    QCOMPARE(anchorCurve()->nodes().count(), 3);
    const auto *inserted = anchorCurve()->nodes().toList().at(1);
    const auto tickPerPixel = (canvas.endTick() - canvas.startTick()) / canvas.width();
    QVERIFY(qAbs(inserted->pos() - 960) <= tickPerPixel);
    QCOMPARE(inserted->value(), 6200);
    const auto insertedTick = inserted->pos();
    const auto insertedId = inserted->id();
    QCOMPARE(fixture.runtime().documentVersion().revision, before.revision + 1);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    fixture.frameAfter(insertedFrame);
    if (QTest::currentTestFailed())
        return;
    const auto *historyEntry = historyManager->nextUndoEntry();
    const auto beforeCancel = fixture.runtime().documentVersion();
    const auto press = fixture.pointFor(insertedTick, 62);
    const auto release = fixture.pointFor(insertedTick + 240, 61);
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, press);
    fixture.moveTo(release);
    QVERIFY(editSessionManager->hasActiveTransaction());
    QCOMPARE(inserted->pos(), insertedTick);
    QCOMPARE(inserted->value(), 6200);
    const auto canceledFrame = fixture.submitted->size();
    QTest::keyClick(&canvas, Qt::Key_Escape);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, release);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(fixture.runtime().documentVersion(), beforeCancel);
    QCOMPARE(historyManager->nextUndoEntry(), historyEntry);
    const auto *restored = anchorCurve()->nodes().toList().at(1);
    QCOMPARE(restored->id(), insertedId);
    QCOMPARE(restored->pos(), insertedTick);
    QCOMPARE(restored->value(), 6200);
    fixture.frameAfter(canceledFrame);
    if (QTest::currentTestFailed())
        return;
    const auto undoFrame = fixture.submitted->size();
    QVERIFY(fixture.runtime().history().undo(fixture.command()));
    QVERIFY(anchorCurve());
    QCOMPARE(anchorCurve()->nodes().count(), 2);
    QCOMPARE(anchorCurve()->nodes().toList().first()->pos(), 480);
    QCOMPARE(anchorCurve()->nodes().toList().last()->pos(), 1440);
    QVERIFY(!historyManager->canUndo());
    fixture.frameAfter(undoFrame);
}
