#include "tst_native_desktop.h"
#include "NativeAppFixture.h"

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
#include <TalcsDevice/AudioDevice.h>

#include <QApplication>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

void NativeDesktopTests::rhiNoteDrawingCommitsAndUndoUpdatesInteraction() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    NativeAppFixture fixture;
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
