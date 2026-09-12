#include "tst_native_desktop.h"
#include "../TestSupport/GuiAppFixture.h"
#include "../TestSupport/VoicebankFixture.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Bootstrap/AppEnvironment.h"
#include "Controller/ClipController.h"
#include "Controller/TrackController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/OutputSystem.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollRhiWidget.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollCoord.h"
#include "UI/Views/ClipEditor/ClipEditorView.h"
#include "UI/Views/TrackEditor/TracksRhiWidget.h"
#include "UI/Views/Common/TimelineView.h"
#include "UI/Window/MainWindow.h"
#include "UI/Dialogs/Base/Dialog.h"

#include <lite/GUI/Controls/Toast.h>

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
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/PackageManager/PackageManager.h>
#include <lite/Tasking/TaskManager.h>
#include <TalcsDevice/AudioDevice.h>

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QCursor>
#include <QFileInfo>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QWindow>
#include <QtTest/QTest>

#include <algorithm>

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
            QCursor::setPos(previousCursor);
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
            TestSupport::placeWindowOnScreen(*canvas, {900, 500});
            canvas->show();
            canvas->activateWindow();
            canvas->setFocus();
            QTRY_VERIFY(canvas->isActiveWindow() && canvas->hasFocus());
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

        void addSecondNote() {
            Automation::NoteDraftDto draft;
            draft.localStart = 1200;
            draft.length = 480;
            draft.keyIndex = 62;
            draft.lyric = QStringLiteral("li");
            draft.language = QStringLiteral("eng");
            const auto beforeFrame = submitted->size();
            QVERIFY(
                runtime().notes().insertNotes(command(), Automation::ClipId(clip->id()), {draft}));
            QCOMPARE(clip->notes().count(), 2);
            for (const auto *note : clip->notes()) {
                if (note->id() != noteId)
                    secondNoteId = note->id();
            }
            QVERIFY(secondNoteId >= 0);
            frameAfter(beforeFrame);
            historyManager->reset();
        }

        void moveTo(const QPoint &position) const {
            // Edge scrolling reads the native cursor while waiting for a frame.
            QCursor::setPos(canvas->mapToGlobal(position));
            // Deliver pending platform movement before the synthetic drag position.
            QCoreApplication::processEvents();
            QTest::mouseMove(canvas.get(), position);
        }

        void frameAfter(qsizetype count) const {
            QTRY_VERIFY(submitted->size() > count || !backendError.isEmpty());
            QVERIFY2(backendError.isEmpty(), qPrintable(backendError));
        }

        GuiDocumentFixture app;
        SingingClip *clip = nullptr;
        int noteId = -1;
        int secondNoteId = -1;
        std::unique_ptr<PianoRollRhiWidget> canvas;
        std::unique_ptr<QSignalSpy> submitted;
        QString backendError;
        QPoint previousCursor = QCursor::pos();
    };
}

void NativeDesktopTests::rhiThemeSwitchPreservesBothEditorsAndTheirDocument() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    GuiDocumentFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.error));
    auto *themes = ThemeManager::instance();
    const auto previousTheme = themes->currentThemeId();
    const auto backend = appOptions->developer()->editorRenderBackend;
    const auto nativeFrame = appOptions->appearance()->useNativeFrame;
    const auto directManipulation = appOptions->appearance()->enableDirectManipulation;
    const auto restore = qScopeGuard([&] {
        appOptions->developer()->editorRenderBackend = backend;
        appOptions->appearance()->useNativeFrame = nativeFrame;
        appOptions->appearance()->enableDirectManipulation = directManipulation;
        themes->applyTheme(previousTheme);
    });
    appOptions->developer()->editorRenderBackend =
        DeveloperOption::EditorRenderBackend::RhiExperimental;
    appOptions->appearance()->useNativeFrame = true;
    appOptions->appearance()->enableDirectManipulation = false;
    QVERIFY2(themes->applyTheme(ThemeIds::defaultThemeId()), qPrintable(ThemeLoader::lastError()));
    MainWindow window;
    const auto detach = qScopeGuard([&] {
        clipController->setClip(nullptr);
        trackController->setParentWidget(nullptr);
        Dialog::setGlobalContext(nullptr);
        Toast::setGlobalContext(nullptr);
    });
    auto *tracks = window.findChild<TracksRhiWidget *>();
    auto *piano = window.findChild<PianoRollRhiWidget *>();
    auto *editor = window.findChild<ClipEditorView *>();
    auto *pianoTimeline = window.findChild<TimelineView *>("pianoRollTimelineView");
    auto *tracksTimeline = window.findChild<TimelineView *>("tracksTimelineView");
    QVERIFY(tracks && piano && editor && pianoTimeline && tracksTimeline);
    QSignalSpy trackFrames(tracks, &QRhiWidget::frameSubmitted);
    QSignalSpy pianoFrames(piano, &QRhiWidget::frameSubmitted);
    QSignalSpy trackErrors(tracks, &QRhiWidget::renderFailed);
    QSignalSpy pianoErrors(piano, &QRhiWidget::renderFailed);
    auto &runtime = *fixture.context->m_coreRuntime;
    const auto command = [&] {
        return Automation::CommandContext{.expected = runtime.documentVersion(),
                                          .source = Automation::InvocationSource::Test};
    };
    Automation::NoteDraftDto note;
    note.localStart = 480;
    note.length = 480;
    note.keyIndex = 60;
    note.lyric = QStringLiteral("la");
    note.language = QStringLiteral("eng");
    Automation::ClipDraftDto draft;
    draft.properties.length = 3840;
    draft.properties.clipLen = 3840;
    draft.defaultLanguage = note.language;
    draft.notes = {note};
    Automation::TrackDraftDto track;
    track.name = QStringLiteral("Theme switch");
    track.clips = {draft};
    QVERIFY(runtime.project().insertTrack(command(), 0, track));
    auto *clip = dynamic_cast<SingingClip *>(
        *fixture.context->m_appModel->tracks().first()->clips().begin());
    QVERIFY(clip);
    appStatus->activeClipId = clip->id();
    TestSupport::placeWindowOnScreen(window, {1200, 900});
    window.show();
    window.activateWindow();
    QVERIFY(window.setEditorPanelVisibility(true, true));
    QVERIFY(window.showBottomPanelPage(QStringLiteral("ClipEditor")));
    QTRY_VERIFY(window.isActiveWindow());
    QTRY_VERIFY((!trackFrames.isEmpty() && !pianoFrames.isEmpty()) || !trackErrors.isEmpty() ||
                !pianoErrors.isEmpty());
    QVERIFY(trackErrors.isEmpty() && pianoErrors.isEmpty());
    QVERIFY(window.setPianoRollScale(1, 1));
    QVERIFY(window.centerPianoRollAt(1920, 60));
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto model = fixture.context->m_appModel->serialize();
    const auto darkPiano = piano->property("whiteKeyColor").value<QColor>();
    const auto darkTracks = tracks->property("backgroundColor").value<QColor>();
    const auto verifyTimelines = [&] {
        // QSS serializes semantic colors to 8-bit channels.
        for (const auto *timeline : {pianoTimeline, tracksTimeline}) {
            QCOMPARE(timeline->palette().color(QPalette::Window).rgba(),
                     themes->semanticColor(QStringLiteral("timeline.background")).rgba());
            QCOMPARE(timeline->property("barScaleColor").value<QColor>().rgba(),
                     themes->semanticColor(QStringLiteral("editor.playhead")).rgba());
        }
    };
    verifyTimelines();
    if (QTest::currentTestFailed())
        return;
    const auto pianoFrame = pianoFrames.size();
    const auto trackFrame = trackFrames.size();
    QVERIFY2(themes->applyTheme(ThemeIds::lightThemeId()), qPrintable(ThemeLoader::lastError()));
    QTRY_VERIFY(piano->property("whiteKeyColor").value<QColor>() != darkPiano &&
                tracks->property("backgroundColor").value<QColor>() != darkTracks);
    QTRY_VERIFY(pianoFrames.size() > pianoFrame && trackFrames.size() > trackFrame);
    QVERIFY(piano->property("whiteKeyColor").value<QColor>().isValid() &&
            tracks->property("backgroundColor").value<QColor>().isValid());
    verifyTimelines();
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(fixture.context->m_appModel->serialize(), model);
    QCOMPARE(appStatus->activeClipId.get(), clip->id());
    QVERIFY(!historyManager->canUndo());
    QVERIFY2(themes->applyTheme(ThemeIds::defaultThemeId()), qPrintable(ThemeLoader::lastError()));
    QTRY_COMPARE(piano->property("whiteKeyColor").value<QColor>(), darkPiano);
    QTRY_COMPARE(tracks->property("backgroundColor").value<QColor>(), darkTracks);
    verifyTimelines();
    if (QTest::currentTestFailed())
        return;

    QVERIFY(editor->setEditMode(EditorViewGlobal::DrawNote));
    QVERIFY(editor->setRegionVisibility(true, false));
    QVERIFY(window.centerPianoRollAt(1440, 60));
    QVERIFY(window.focusEditorRegion(EditorViewGlobal::Region::PianoRoll));
    QTRY_VERIFY(piano->height() > 0 && piano->width() > 0);
    QTest::mouseClick(piano, Qt::LeftButton, Qt::NoModifier, piano->rect().center());
    QCOMPARE(clip->notes().count(), 2);
    QVERIFY(runtime.history().undo(command()));
    QCOMPARE(fixture.context->m_appModel->serialize(), model);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(trackErrors.isEmpty() && pianoErrors.isEmpty());
}

void NativeDesktopTests::rhiNoteDrawingCommitsAndUndoUpdatesInteraction() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    GuiDocumentFixture fixture;
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

void NativeDesktopTests::rhiNoteDragKeepsScrollingUntilTheGestureEnds() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    ExistingRhiNoteFixture fixture;
    fixture.initialize();
    if (QTest::currentTestFailed())
        return;
    auto &canvas = *fixture.canvas;
    const auto oldCursor = QCursor::pos();
    const auto restoreCursor = qScopeGuard([&] { QCursor::setPos(oldCursor); });
    const auto before = fixture.runtime().documentVersion();
    const auto original = fixture.app.context->m_appModel->serialize();
    const auto initialStart = canvas.startTick();
    const auto press = fixture.pointFor(720, 60);
    const auto edge = QPoint(canvas.width() - 2, press.y());
    QTest::mousePress(canvas.windowHandle(), Qt::LeftButton, Qt::NoModifier, press);
    QCursor::setPos(canvas.mapToGlobal(edge));
    QTest::mouseMove(canvas.windowHandle(), edge);
    QVERIFY(editSessionManager->hasActiveTransaction());
    const auto afterMove = canvas.startTick();
    QTRY_VERIFY_WITH_TIMEOUT(canvas.startTick() > afterMove + 60, 3000);
    QVERIFY(!appStatus->pianoRollNoteEditPreview.get().isEmpty());
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QCOMPARE(fixture.app.context->m_appModel->serialize(), original);
    QTest::keyClick(&canvas, Qt::Key_Escape);
    QTest::mouseRelease(canvas.windowHandle(), Qt::LeftButton, Qt::NoModifier, edge);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
    QTest::qWait(80);
    QCOMPARE(canvas.startTick(), initialStart);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QCOMPARE(fixture.app.context->m_appModel->serialize(), original);
    QVERIFY(!historyManager->canUndo());
}

void NativeDesktopTests::rhiNoteMoveCanBeCanceledAndThenCommitted() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    ExistingRhiNoteFixture fixture;
    fixture.initialize();
    if (QTest::currentTestFailed())
        return;
    auto &canvas = *fixture.canvas;
    QVERIFY(canvas.setViewScale(2, 1));
    QVERIFY(canvas.centerAt(960, 60));
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
    const auto pageStart = canvas.startTick();
    const auto nextPagePosition = canvas.endTick() + 120;
    QVERIFY(nextPagePosition < fixture.clip->length());
    canvas.setAutoPageTurn(true);
    canvas.setPlaybackPosition(nextPagePosition);
    QCOMPARE(canvas.startTick(), pageStart);
    canvas.setAutoPageTurn(false);
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
    const auto afterEditing = fixture.runtime().documentVersion();
    canvas.setAutoPageTurn(true);
    QTRY_VERIFY2(canvas.startTick() > pageStart && canvas.startTick() <= nextPagePosition &&
                     canvas.endTick() >= nextPagePosition,
                 qPrintable(QStringLiteral("Playback %1, viewport %2..%3, previous start %4")
                                .arg(nextPagePosition)
                                .arg(canvas.startTick())
                                .arg(canvas.endTick())
                                .arg(pageStart)));
    canvas.setPlaybackPosition(480);
    QTRY_VERIFY(canvas.startTick() <= 480 && canvas.endTick() >= 480);
    QCOMPARE(fixture.runtime().documentVersion(), afterEditing);
    QCOMPARE(note->localStart(), 480);
    QVERIFY(!historyManager->canUndo());
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
    const auto beforeAppend = fixture.app.context->m_appModel->serialize();
    const auto appendPosition = fixture.pointFor(1680, 61);
    const auto hoverFrame = fixture.submitted->size();
    QTest::mouseMove(canvas.windowHandle(), appendPosition);
    fixture.frameAfter(hoverFrame);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(fixture.app.context->m_appModel->serialize(), beforeAppend);
    QCOMPARE(anchorCurve()->nodes().toList().last()->interpMode(), AnchorNode::None);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, appendPosition);
    QCOMPARE(anchorCurve()->nodes().count(), 4);
    const auto appendedNodes = anchorCurve()->nodes().toList();
    QVERIFY(qAbs(appendedNodes.last()->pos() - 1680) <= tickPerPixel);
    QCOMPARE(appendedNodes.last()->value(), 6100);
    QCOMPARE(appendedNodes.last()->interpMode(), AnchorNode::None);
    QCOMPARE(appendedNodes.at(2)->interpMode(), AnchorNode::Hermite);
    QVERIFY(fixture.runtime().history().undo(fixture.command()));
    QCOMPARE(fixture.app.context->m_appModel->serialize(), beforeAppend);
    inserted = anchorCurve()->nodes().toList().at(1);
    const auto *historyEntry = historyManager->nextUndoEntry();
    const auto beforeCancel = fixture.runtime().documentVersion();
    const auto press = fixture.pointFor(insertedTick, 62);
    const auto release = fixture.pointFor(insertedTick + 240, 61);
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, press);
    const auto previewFrame = fixture.submitted->size();
    fixture.moveTo(release);
    fixture.frameAfter(previewFrame);
    if (QTest::currentTestFailed())
        return;
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

void NativeDesktopTests::rhiNoteEraseStrokeCancelsAndCommitsAtomically() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    ExistingRhiNoteFixture fixture;
    fixture.initialize();
    if (QTest::currentTestFailed())
        return;
    fixture.addSecondNote();
    if (QTest::currentTestFailed())
        return;
    auto &canvas = *fixture.canvas;
    canvas.setEditMode(ClipEditorGlobal::EraseNote);
    const auto first = fixture.pointFor(720, 60);
    const auto second = fixture.pointFor(1440, 62);
    QVERIFY(canvas.rect().contains(first) && canvas.rect().contains(second));
    const QList<int> erased{fixture.noteId, fixture.secondNoteId};
    const auto before = fixture.runtime().documentVersion();
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, first);
    fixture.moveTo(second);
    QCOMPARE(appStatus->pianoRollNoteErasePreview.get(), erased);
    QVERIFY(editSessionManager->hasActiveTransaction());
    QCOMPARE(fixture.clip->notes().count(), 2);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QTest::keyClick(&canvas, Qt::Key_Escape);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, second);
    QVERIFY(appStatus->pianoRollNoteErasePreview.get().isEmpty());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(fixture.clip->notes().count(), 2);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QVERIFY(!historyManager->canUndo());

    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, first);
    fixture.moveTo(second);
    const auto committedFrame = fixture.submitted->size();
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, second);
    QCOMPARE(fixture.clip->notes().count(), 0);
    QVERIFY(appStatus->pianoRollNoteErasePreview.get().isEmpty());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(fixture.runtime().documentVersion().revision, before.revision + 1);
    fixture.frameAfter(committedFrame);
    if (QTest::currentTestFailed())
        return;
    const auto undoFrame = fixture.submitted->size();
    historyManager->undo();
    QCOMPARE(fixture.clip->notes().count(), 2);
    QVERIFY(fixture.clip->findNoteById(fixture.noteId));
    QVERIFY(fixture.clip->findNoteById(fixture.secondNoteId));
    QVERIFY(!historyManager->canUndo());
    fixture.frameAfter(undoFrame);
    if (QTest::currentTestFailed())
        return;
    canvas.setEditMode(ClipEditorGlobal::Select);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, first);
    QCOMPARE(appStatus->selectedNotes.get(), QList<int>{fixture.noteId});
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, second);
    QCOMPARE(appStatus->selectedNotes.get(), QList<int>{fixture.secondNoteId});
}

void NativeDesktopTests::rhiInlineTextEditingNavigatesCancelsAndUndoes() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    ExistingRhiNoteFixture fixture;
    fixture.initialize();
    if (QTest::currentTestFailed())
        return;
    fixture.addSecondNote();
    if (QTest::currentTestFailed())
        return;
    auto &canvas = *fixture.canvas;
    auto *first = fixture.clip->findNoteById(fixture.noteId);
    auto *second = fixture.clip->findNoteById(fixture.secondNoteId);
    QVERIFY(first && second);
    QVERIFY(fixture.runtime().notes().setPronunciation(
        fixture.command(), Automation::ClipId(fixture.clip->id()), Automation::NoteId(first->id()),
        true, QStringLiteral("la")));
    historyManager->reset();
    const auto beginEditing = [&](const QPoint &position, const QString &role) {
        QVERIFY(canvas.rect().contains(position));
        QTest::mouseDClick(&canvas, Qt::LeftButton, Qt::NoModifier, position);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, position);
        auto *edit = canvas.findChild<QLineEdit *>();
        QVERIFY(edit);
        QTRY_VERIFY(edit->isVisible() && edit->hasFocus());
        QCOMPARE(edit->property("editRole").toString(), role);
    };
    beginEditing(fixture.pointFor(720, 60), QStringLiteral("Lyric"));
    if (QTest::currentTestFailed())
        return;
    auto *edit = canvas.findChild<QLineEdit *>();
    QCOMPARE(edit->text(), QStringLiteral("la"));
    QTest::keySequence(edit, QKeySequence::SelectAll);
    QTest::keyClicks(edit, "hello");
    const auto beforeText = fixture.submitted->size();
    QTest::keyClick(edit, Qt::Key_Tab);
    QCOMPARE(first->lyric(), QStringLiteral("hello"));
    QTRY_VERIFY(edit->isVisible() && edit->hasFocus());
    QCOMPARE(edit->text(), QStringLiteral("li"));
    QCOMPARE(appStatus->selectedNotes.get(), QList<int>{second->id()});
    QTest::keySequence(edit, QKeySequence::SelectAll);
    QTest::keyClicks(edit, "world");
    QTest::keyClick(edit, Qt::Key_Backtab);
    QCOMPARE(second->lyric(), QStringLiteral("world"));
    QTRY_VERIFY(edit->isVisible() && edit->hasFocus());
    QCOMPARE(edit->text(), QStringLiteral("hello"));
    QCOMPARE(appStatus->selectedNotes.get(), QList<int>{first->id()});
    const auto beforeCancel = fixture.runtime().documentVersion();
    QTest::keySequence(edit, QKeySequence::SelectAll);
    QTest::keyClicks(edit, "discard this");
    QTest::keyClick(edit, Qt::Key_Escape);
    QTRY_VERIFY(!edit->isVisible());
    QCOMPARE(first->lyric(), QStringLiteral("hello"));
    QCOMPARE(fixture.runtime().documentVersion(), beforeCancel);
    fixture.frameAfter(beforeText);
    if (QTest::currentTestFailed())
        return;
    historyManager->undo();
    QCOMPARE(second->lyric(), QStringLiteral("li"));
    historyManager->undo();
    QCOMPARE(first->lyric(), QStringLiteral("la"));
    QVERIFY(!historyManager->canUndo());

    const auto pronunciationPosition =
        fixture.pointFor(720, 60) +
        QPoint(0, qRound(ClipEditorGlobal::noteHeight * canvas.scaleY() / 2.0) + 8);
    beginEditing(pronunciationPosition, QStringLiteral("Pronunciation"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(edit->text(), QStringLiteral("la"));
    QTest::keySequence(edit, QKeySequence::SelectAll);
    QTest::keyClicks(edit, "lu");
    QTest::keyClick(edit, Qt::Key_Return);
    QTRY_VERIFY(!edit->isVisible());
    QCOMPARE(first->pronunciation().edited, QStringLiteral("lu"));
    QCOMPARE(first->pronunciation().result(), QStringLiteral("lu"));
    historyManager->undo();
    QCOMPARE(first->pronunciation().result(), QStringLiteral("la"));
    QVERIFY(!first->pronunciation().isEdited());
    QVERIFY(!historyManager->canUndo());
    beginEditing(pronunciationPosition, QStringLiteral("Pronunciation"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(edit->text(), QStringLiteral("la"));
    QTest::keyClick(edit, Qt::Key_Escape);
}

void NativeDesktopTests::rhiPitchStrokePreviewsCancelAndCommit_data() {
    QTest::addColumn<EditorViewGlobal::PianoRollEditMode>("mode");
    QTest::newRow("draw") << EditorViewGlobal::DrawPitch;
    QTest::newRow("trace-original") << EditorViewGlobal::TracePitch;
    QTest::newRow("erase") << EditorViewGlobal::ErasePitch;
}

void NativeDesktopTests::rhiPitchStrokePreviewsCancelAndCommit() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    QFETCH(EditorViewGlobal::PianoRollEditMode, mode);
    ExistingRhiNoteFixture fixture;
    fixture.initialize();
    if (QTest::currentTestFailed())
        return;
    auto &canvas = *fixture.canvas;
    Automation::CurveDraftDto original;
    original.values = QList<int>(385, 6000);
    auto edited = original;
    edited.values.fill(6100);
    const auto clipId = Automation::ClipId(fixture.clip->id());
    QVERIFY(fixture.runtime().parameters().replaceParameter(
        fixture.command(), clipId, ParamInfo::Pitch, Param::Original, {original}));
    QVERIFY(fixture.runtime().parameters().replaceParameter(
        fixture.command(), clipId, ParamInfo::Pitch, Param::Edited, {edited}));
    auto *pitch = fixture.clip->params.getParamByName(ParamInfo::Pitch);
    QVERIFY(pitch);
    const auto *originalCurve =
        dynamic_cast<const DrawCurve *>(pitch->curves(Param::Original).first());
    const auto *editedCurve = dynamic_cast<const DrawCurve *>(pitch->curves(Param::Edited).first());
    QVERIFY(originalCurve && editedCurve);
    const DrawCurve originalBefore(*originalCurve);
    const DrawCurve editedBefore(*editedCurve);
    canvas.setEditMode(mode);
    historyManager->reset();
    const auto before = fixture.runtime().documentVersion();
    const auto start = fixture.pointFor(480, 61);
    const auto finish = fixture.pointFor(960, 63);
    QVERIFY(canvas.rect().contains(start) && canvas.rect().contains(finish));
    const auto unchanged = [&] {
        QCOMPARE(pitch->curves(Param::Edited).size(), 1);
        const auto *curve = dynamic_cast<const DrawCurve *>(pitch->curves(Param::Edited).first());
        QVERIFY(curve);
        QCOMPARE(*curve, editedBefore);
    };
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, start);
    const auto previewFrame = fixture.submitted->size();
    fixture.moveTo(finish);
    QVERIFY(editSessionManager->hasActiveTransaction());
    unchanged();
    QCOMPARE(fixture.runtime().documentVersion(), before);
    fixture.frameAfter(previewFrame);
    if (QTest::currentTestFailed())
        return;
    QTest::keyClick(&canvas, Qt::Key_Escape);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, finish);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    unchanged();
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QVERIFY(!historyManager->canUndo());

    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, start);
    fixture.moveTo(finish);
    const auto committedFrame = fixture.submitted->size();
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, finish);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(fixture.runtime().documentVersion().revision, before.revision + 1);
    const auto sampleAt = [&](int tick) -> std::optional<int> {
        for (const auto *curve : pitch->curves(Param::Edited)) {
            const auto *draw = dynamic_cast<const DrawCurve *>(curve);
            if (draw && tick >= draw->localStart() && tick < draw->localEndTick())
                return draw->values().at((tick - draw->localStart()) / draw->step);
        }
        return std::nullopt;
    };
    QCOMPARE(sampleAt(200), std::optional<int>(6100));
    QCOMPARE(sampleAt(1500), std::optional<int>(6100));
    if (mode == EditorViewGlobal::ErasePitch) {
        QVERIFY(!sampleAt(720));
    } else if (mode == EditorViewGlobal::TracePitch) {
        QCOMPARE(sampleAt(720), std::optional<int>(6000));
    } else {
        QVERIFY(sampleAt(720));
        // Integer mouse positions limit precision to one pixel of the pitch scale.
        const auto centsPerPixel = 100.0 / (ClipEditorGlobal::noteHeight * canvas.scaleY());
        QVERIFY(qAbs(*sampleAt(720) - 6200) <= centsPerPixel);
    }
    const auto *currentOriginal =
        dynamic_cast<const DrawCurve *>(pitch->curves(Param::Original).first());
    QVERIFY(currentOriginal);
    QCOMPARE(*currentOriginal, originalBefore);
    fixture.frameAfter(committedFrame);
    if (QTest::currentTestFailed())
        return;
    const auto undoFrame = fixture.submitted->size();
    historyManager->undo();
    unchanged();
    QVERIFY(!historyManager->canUndo());
    fixture.frameAfter(undoFrame);
}

void NativeDesktopTests::rhiNoteSplittingSnapsAndUndoRestoresThePhrase() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    ExistingRhiNoteFixture fixture;
    fixture.initialize();
    if (QTest::currentTestFailed())
        return;
    auto &canvas = *fixture.canvas;
    canvas.setEditMode(ClipEditorGlobal::SplitNote);
    const auto before = fixture.runtime().documentVersion();
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, fixture.pointFor(480, 60));
    QCOMPARE(fixture.clip->notes().count(), 1);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    const auto position = fixture.pointFor(730, 60);
    const auto previewFrame = fixture.submitted->size();
    QTest::mouseMove(canvas.windowHandle(), position);
    fixture.frameAfter(previewFrame);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(fixture.clip->notes().count(), 1);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    const auto beforeSplit = fixture.submitted->size();
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, position);
    QCOMPARE(fixture.clip->notes().count(), 2);
    const auto *first = fixture.clip->findNoteById(fixture.noteId);
    QVERIFY(first);
    QCOMPARE(first->localStart(), 480);
    QCOMPARE(first->length(), 240);
    const Note *continuation = nullptr;
    for (const auto *note : fixture.clip->notes()) {
        if (note->id() != fixture.noteId)
            continuation = note;
    }
    QVERIFY(continuation);
    QCOMPARE(continuation->localStart(), 720);
    QCOMPARE(continuation->length(), 240);
    QCOMPARE(continuation->keyIndex(), first->keyIndex());
    QCOMPARE(continuation->lyric(), QStringLiteral("-"));
    QCOMPARE(continuation->language(), first->language());
    QCOMPARE(fixture.runtime().documentVersion().revision, before.revision + 1);
    fixture.frameAfter(beforeSplit);
    if (QTest::currentTestFailed())
        return;
    const auto undoFrame = fixture.submitted->size();
    historyManager->undo();
    QCOMPARE(fixture.clip->notes().count(), 1);
    const auto *restored = fixture.clip->findNoteById(fixture.noteId);
    QVERIFY(restored);
    QCOMPARE(restored->localStart(), 480);
    QCOMPARE(restored->length(), 480);
    QCOMPARE(restored->lyric(), QStringLiteral("la"));
    QVERIFY(!historyManager->canUndo());
    fixture.frameAfter(undoFrame);
    if (QTest::currentTestFailed())
        return;
    canvas.setEditMode(ClipEditorGlobal::Select);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, fixture.pointFor(840, 60));
    QCOMPARE(appStatus->selectedNotes.get(), QList<int>{fixture.noteId});
}

void NativeDesktopTests::rhiContextMenuTargetsRespectPronunciationAndSelection() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    ExistingRhiNoteFixture fixture;
    fixture.initialize();
    if (QTest::currentTestFailed())
        return;
    fixture.addSecondNote();
    if (QTest::currentTestFailed())
        return;
    auto &canvas = *fixture.canvas;
    QVERIFY(fixture.runtime().notes().setPronunciation(
        fixture.command(), Automation::ClipId(fixture.clip->id()),
        Automation::NoteId(fixture.noteId), true, QStringLiteral("la")));
    historyManager->reset();
    const auto before = fixture.runtime().documentVersion();
    QList<PianoRollMenuContext> menus;
    connect(&canvas, &PianoRollRhiWidget::contextMenuRequested, &canvas,
            [&](const PianoRollMenuContext &menu) { menus.append(menu); });
    const auto requestAt = [&](const QPoint &position) {
        QVERIFY(canvas.rect().contains(position));
        const auto count = menus.size();
        QContextMenuEvent event(QContextMenuEvent::Mouse, position, canvas.mapToGlobal(position));
        QApplication::sendEvent(&canvas, &event);
        QCOMPARE(menus.size(), count + 1);
    };
    const auto first = fixture.pointFor(720, 60);
    const auto second = fixture.pointFor(1440, 62);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, first);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::ControlModifier, second);
    const auto selected = appStatus->selectedNotes.get();
    QCOMPARE(selected.size(), 2);
    requestAt(first);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(menus.last().target, PianoRollMenuContext::Target::Note);
    QCOMPARE(menus.last().noteId, fixture.noteId);
    QCOMPARE(menus.last().selectedNoteIds, selected);
    QVERIFY(!menus.last().pronunciationTarget);
    QCOMPARE(appStatus->selectedNotes.get(), selected);

    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, fixture.pointFor(2000, 70));
    QVERIFY(appStatus->selectedNotes.get().isEmpty());
    const auto pronunciation =
        first + QPoint(0, qRound(ClipEditorGlobal::noteHeight * canvas.scaleY() / 2.0) + 8);
    requestAt(pronunciation);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(menus.last().pronunciationTarget);
    QCOMPARE(menus.last().target, PianoRollMenuContext::Target::Note);
    QCOMPARE(menus.last().noteId, fixture.noteId);
    QCOMPARE(menus.last().selectedNoteIds, QList<int>{fixture.noteId});
    QCOMPARE(menus.last().noteLanguage, QStringLiteral("eng"));
    QCOMPARE(appStatus->selectedNotes.get(), QList<int>{fixture.noteId});
    requestAt(fixture.pointFor(2000, 70));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(menus.last().target, PianoRollMenuContext::Target::Background);
    QCOMPARE(menus.last().keyIndex, 70);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}

void NativeDesktopTests::rhiMultiNoteSelectionAndMoveCommitAtomically() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    ExistingRhiNoteFixture fixture;
    fixture.initialize();
    if (QTest::currentTestFailed())
        return;
    fixture.addSecondNote();
    if (QTest::currentTestFailed())
        return;
    auto &canvas = *fixture.canvas;
    auto *first = fixture.clip->findNoteById(fixture.noteId);
    auto *second = fixture.clip->findNoteById(fixture.secondNoteId);
    QVERIFY(first && second);
    const auto before = fixture.runtime().documentVersion();
    const auto selectRange = [&](double upperKey, double lowerKey) {
        const auto start = fixture.pointFor(240, upperKey);
        const auto end = fixture.pointFor(1800, lowerKey);
        QVERIFY(canvas.rect().contains(start) && canvas.rect().contains(end));
        const auto frame = fixture.submitted->size();
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, start);
        fixture.moveTo(end);
        fixture.frameAfter(frame);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, end);
        const auto selected = appStatus->selectedNotes.get();
        QCOMPARE(selected.size(), 2);
        QVERIFY(selected.contains(first->id()) && selected.contains(second->id()));
    };
    selectRange(64, 58);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::ControlModifier, fixture.pointFor(1440, 62));
    QCOMPARE(appStatus->selectedNotes.get(), QList<int>{first->id()});
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::ControlModifier, fixture.pointFor(1440, 62));
    QCOMPARE(appStatus->selectedNotes.get().size(), 2);
    const auto start = fixture.pointFor(720, 60);
    const auto end = fixture.pointFor(1200, 61);
    for (bool commit : {false, true}) {
        const auto frame = fixture.submitted->size();
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, start);
        fixture.moveTo(end);
        QVERIFY(editSessionManager->hasActiveTransaction());
        QCOMPARE(first->localStart(), 480);
        QCOMPARE(second->localStart(), 1200);
        QCOMPARE(fixture.runtime().documentVersion(), before);
        fixture.frameAfter(frame);
        if (QTest::currentTestFailed())
            return;
        if (!commit)
            QTest::keyClick(&canvas, Qt::Key_Escape);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, end);
        QVERIFY(!editSessionManager->hasActiveTransaction());
        QCOMPARE(first->localStart(), commit ? 960 : 480);
        QCOMPARE(second->localStart(), commit ? 1680 : 1200);
        QCOMPARE(first->keyIndex(), commit ? 61 : 60);
        QCOMPARE(second->keyIndex(), commit ? 63 : 62);
        QCOMPARE(historyManager->canUndo(), commit);
    }
    QCOMPARE(fixture.runtime().documentVersion().revision, before.revision + 1);
    historyManager->undo();
    QCOMPARE(first->localStart(), 480);
    QCOMPARE(second->localStart(), 1200);
    QCOMPARE(first->keyIndex(), 60);
    QCOMPARE(second->keyIndex(), 62);
    QVERIFY(!historyManager->canUndo());
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, fixture.pointFor(240, 64));
    QVERIFY(appStatus->selectedNotes.get().isEmpty());
    canvas.setEditMode(ClipEditorGlobal::IntervalSelect);
    selectRange(64, 63);
    QVERIFY(!historyManager->canUndo());
}

void NativeDesktopTests::rhiPianoMenuPasteAndVisibilityUseTheFullEditor() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    GuiDocumentFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.error));
    auto &runtime = *fixture.context->m_coreRuntime;
    const auto command = [&] {
        return Automation::CommandContext{.expected = runtime.documentVersion(),
                                          .source = Automation::InvocationSource::Test};
    };
    Automation::NoteDraftDto note;
    note.localStart = 480;
    note.length = 480;
    note.keyIndex = 60;
    note.lyric = QStringLiteral("la");
    note.language = QStringLiteral("eng");
    note.pronunciation.edited = QStringLiteral("lah");
    Automation::ClipDraftDto draft;
    draft.properties.length = 3840;
    draft.properties.clipLen = 3840;
    draft.defaultLanguage = note.language;
    draft.notes = {note};
    Automation::TrackDraftDto track;
    track.clips = {draft};
    auto document = Automation::DocumentAutomationFacade::newDocumentDraft(false);
    document.tracks = {track};
    QVERIFY(runtime.documents().commitNewDocument(command(), document));
    auto *clip = dynamic_cast<SingingClip *>(
        *fixture.context->m_appModel->tracks().first()->clips().begin());
    QVERIFY(clip);
    const auto sourceId = (*clip->notes().begin())->id();
    clipController->setClip(clip);
    appStatus->activeClipId = clip->id();
    appOptions->developer()->editorRenderBackend =
        DeveloperOption::EditorRenderBackend::RhiExperimental;
    PianoRollView editor;
    auto *canvas = editor.findChild<PianoRollRhiWidget *>();
    QVERIFY(canvas);
    canvas->setApi(QRhiWidget::Api::Null);
    editor.setDataContext(clip);
    const auto clearContext = qScopeGuard([&] {
        editor.setDataContext(nullptr);
        clipController->setClip(nullptr);
    });
    auto clipboard = std::make_unique<QMimeData>();
    if (const auto *mime = QApplication::clipboard()->mimeData()) {
        for (const auto &format : mime->formats())
            clipboard->setData(format, mime->data(format));
    }
    const auto previousCursor = QCursor::pos();
    const auto restore = qScopeGuard([&] {
        QApplication::clipboard()->setMimeData(clipboard.release());
        QCursor::setPos(previousCursor);
    });
    QSignalSpy frames(canvas, &QRhiWidget::frameSubmitted);
    QSignalSpy failed(canvas, &QRhiWidget::renderFailed);
    editor.resize(1000, 550);
    editor.show();
    editor.activateWindow();
    QTRY_VERIFY(editor.isActiveWindow() && !frames.isEmpty());
    QVERIFY(editor.setViewScale(1, 1));
    QVERIFY(editor.centerAt(1920, 60));
    QVERIFY(editor.focusEditor());
    QVERIFY(failed.isEmpty());
    const auto point = [&](int tick, int key) {
        return QPoint(qRound((tick - canvas->startTick()) * canvas->width() /
                             (canvas->endTick() - canvas->startTick())),
                      qRound(canvas->height() / 2.0 + (canvas->centerKeyIndex() - key) *
                                                          ClipEditorGlobal::noteHeight *
                                                          canvas->scaleY()));
    };
    const auto before = runtime.documentVersion();
    historyManager->reset();
    const auto runMenu = [&](QPoint position, const QString &text, bool paste, bool commit) {
        bool entered = false;
        QTimer respond;
        respond.setSingleShot(true);
        connect(&respond, &QTimer::timeout, &editor, [&] {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(menu);
            const auto close = qScopeGuard([&] { menu->close(); });
            entered = true;
            QAction *action = nullptr;
            for (auto *item : menu->actions()) {
                if (item->text() == text)
                    action = item;
            }
            QVERIFY(action && action->isEnabled());
            const auto target = menu->actionGeometry(action).center();
            if (paste) {
                const auto frame = frames.size();
                QTest::mouseMove(menu->windowHandle(), target);
                QTRY_COMPARE(menu->activeAction(), action);
                QTRY_VERIFY(frames.size() > frame);
                QCOMPARE(clip->notes().count(), 1);
                QCOMPARE(runtime.documentVersion(), before);
                QVERIFY(!historyManager->canUndo());
            }
            if (commit)
                QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, target);
            else
                QTest::keyClick(menu, Qt::Key_Escape);
        });
        const auto global = canvas->mapToGlobal(position);
        QTest::mouseMove(editor.windowHandle(), editor.mapFromGlobal(global));
        QContextMenuEvent event(QContextMenuEvent::Mouse, position, global);
        respond.start(0);
        QApplication::sendEvent(canvas, &event);
        QVERIFY(entered);
    };
    runMenu(point(720, 60), PianoRollContextMenuController::tr("&Copy"), false, true);
    if (QTest::currentTestFailed())
        return;
    for (bool commit : {false, true}) {
        runMenu(point(1920, 64), PianoRollContextMenuController::tr("&Paste"), true, commit);
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(clip->notes().count(), commit ? 2 : 1);
    }
    Note *pasted = nullptr;
    for (auto *candidate : clip->notes()) {
        if (candidate->id() != sourceId)
            pasted = candidate;
    }
    QVERIFY(pasted);
    QCOMPARE(pasted->localStart(), 1920);
    QCOMPARE(pasted->keyIndex(), note.keyIndex);
    QCOMPARE(pasted->pronunciation().edited, note.pronunciation.edited);
    const auto pastedId = pasted->id();
    const auto *entry = historyManager->nextUndoEntry();
    QVERIFY(entry && entry->focusTransition());
    QVERIFY(editor.setPitchViewport(84, 1));
    QCOMPARE(editor.focusVisibility(entry->focusTransition()->after),
             HistoryFocusVisibility::ScrollRequired);
    QVERIFY(editor.revealFocus(entry->focusTransition()->after, false));
    QCOMPARE(editor.focusVisibility(entry->focusTransition()->after),
             HistoryFocusVisibility::Visible);
    QVERIFY(!appStatus->pianoRollVisibleRect.get().isEmpty());
    editor.hide();
    QVERIFY(appStatus->pianoRollVisibleRect.get().isEmpty());
    editor.show();
    QTRY_VERIFY(!appStatus->pianoRollVisibleRect.get().isEmpty());
    historyManager->undo();
    QCOMPARE(clip->notes().count(), 1);
    QVERIFY(!clip->findNoteById(pastedId));
    QVERIFY(clip->findNoteById(sourceId));
    QVERIFY(!historyManager->canUndo());
    QVERIFY(failed.isEmpty());

    QVERIFY(editor.centerAt(1920, 60));
    Automation::CurveDraftDto anchors;
    anchors.type = Automation::CurveDraftDto::Type::Anchor;
    anchors.nodes = {
        {480,  6000, AnchorNode::Hermite},
        {960,  6100, AnchorNode::Hermite},
        {1440, 6000, AnchorNode::None   }
    };
    QVERIFY(runtime.parameters().replaceParameter(command(), Automation::ClipId(clip->id()),
                                                  ParamInfo::Pitch, Param::Edited, {anchors}));
    auto *pitch = clip->params.getParamByName(ParamInfo::Pitch);
    QVERIFY(pitch);
    const auto anchorNodes = [&] {
        return dynamic_cast<const AnchorCurve *>(pitch->curves(Param::Edited).first())
            ->nodes()
            .toList();
    };
    const auto beforeAnchorMenu = fixture.context->m_appModel->serialize();
    editor.onEditModeChanged(ClipEditorGlobal::EditPitchAnchor);
    runMenu(point(960, 61), PianoRollContextMenuController::tr("Linear"), false, true);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(anchorNodes().size(), 3);
    QCOMPARE(anchorNodes().at(0)->interpMode(), AnchorNode::Hermite);
    QCOMPARE(anchorNodes().at(1)->interpMode(), AnchorNode::Linear);
    QCOMPARE(anchorNodes().at(2)->interpMode(), AnchorNode::None);
    runMenu(point(960, 61), PianoRollContextMenuController::tr("&Delete"), false, true);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(anchorNodes().size(), 2);
    QCOMPARE(anchorNodes().first()->pos(), 480);
    QCOMPARE(anchorNodes().last()->pos(), 1440);
    QVERIFY(runtime.history().undo(command()));
    QCOMPARE(anchorNodes().size(), 3);
    QCOMPARE(anchorNodes().at(1)->interpMode(), AnchorNode::Linear);
    QVERIFY(runtime.history().undo(command()));
    QCOMPARE(fixture.context->m_appModel->serialize(), beforeAnchorMenu);
    QVERIFY(runtime.history().undo(command()));
    QVERIFY(pitch->curves(Param::Edited).isEmpty());
    QVERIFY(!historyManager->canUndo());

    editor.onEditModeChanged(ClipEditorGlobal::DrawNote);
    const auto beforeFallback = runtime.documentVersion();
    const auto sourceNote = clip->findNoteById(sourceId)->serialize();
    const auto viewport = editor.viewState();
    const auto onePixelTicks = (canvas->endTick() - canvas->startTick()) / canvas->width();
    QPointer<PianoRollRhiWidget> previousCanvas(canvas);
    canvas->backendFailed(QStringLiteral("Rendering device lost"));
    QTRY_VERIFY(previousCanvas.isNull());
    auto *legacy = editor.findChild<PianoRollGraphicsView *>();
    QVERIFY(legacy);
    QTRY_VERIFY(legacy->isVisible());
    QTRY_COMPARE(editor.viewState().horizontalScale, viewport.horizontalScale);
    QCOMPARE(editor.viewState().verticalScale, viewport.verticalScale);
    QCOMPARE(editor.viewState().editMode, viewport.editMode);
    QTRY_VERIFY(std::abs(editor.viewState().centerTick - viewport.centerTick) <= onePixelTicks);
    QVERIFY(std::abs(editor.viewState().centerKeyIndex - viewport.centerKeyIndex) <=
            1.0 / (ClipEditorGlobal::noteHeight * viewport.verticalScale));
    QCOMPARE(runtime.documentVersion(), beforeFallback);
    editor.activateWindow();
    QTRY_VERIFY(editor.isActiveWindow());
    QTRY_VERIFY(editor.focusEditor());
    const auto legacyPoint = [&](int tick, int key) {
        return legacy->mapFromScene(QPointF(
            legacy->tickToSceneX(tick), PianoRollCoord::keyIndexToCenterY(
                                            key, ClipEditorGlobal::noteHeight * legacy->scaleY())));
    };
    const auto press = legacyPoint(1440, 64);
    const auto release = legacyPoint(1680, 64);
    QVERIFY(legacy->viewport()->rect().contains(press));
    QVERIFY(legacy->viewport()->rect().contains(release));
    QTest::mousePress(legacy->viewport(), Qt::LeftButton, Qt::NoModifier, press);
    QMouseEvent drag(QEvent::MouseMove, QPointF(release),
                     QPointF(legacy->viewport()->mapToGlobal(release)), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(legacy->viewport(), &drag);
    QTest::mouseRelease(legacy->viewport(), Qt::LeftButton, Qt::NoModifier, release);
    QCOMPARE(clip->notes().count(), 2);
    for (const auto *created : clip->notes()) {
        if (created->id() == sourceId)
            QCOMPARE(created->serialize(), sourceNote);
        else {
            QCOMPARE(created->localStart(), 1440);
            QCOMPARE(created->length(), 240);
            QCOMPARE(created->keyIndex(), 64);
        }
    }
    historyManager->undo();
    QCOMPARE(clip->notes().count(), 1);
    QCOMPARE(clip->findNoteById(sourceId)->serialize(), sourceNote);
    QVERIFY(!historyManager->canUndo());
}

void NativeDesktopTests::rhiPitchModulationUsesTheInferredBaseline() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    const auto root = TestSupport::voicebankRoot();
    QVERIFY2(QFileInfo(root).isAbsolute() && QFileInfo(root).isDir(), qPrintable(root));
    QVERIFY(!TestSupport::fixtureLanguage().isEmpty() && !TestSupport::fixtureLyric().isEmpty());
    ExistingRhiNoteFixture fixture;
    fixture.initialize();
    if (QTest::currentTestFailed())
        return;
    packageManager->initialize({root});
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    SingerInfo singer;
    for (const auto &package : packageManager->installedPackages().successfulPackages) {
        for (const auto &candidate : package.singers()) {
            if (candidate.singerId() == TestSupport::fixtureSingerId())
                singer = candidate;
        }
    }
    QVERIFY(!singer.isEmpty() && !singer.speakers().isEmpty());
    auto &runtime = fixture.runtime();
    const auto clipId = Automation::ClipId(fixture.clip->id());
    QVERIFY(runtime.parameters().selectClipSingleSpeaker(fixture.command(), clipId, singer,
                                                         singer.speakers().first()));
    QVERIFY(runtime.notes().patchWordProperties(fixture.command(), clipId,
                                                {
                                                    {.noteId = Automation::NoteId(fixture.noteId),
                                                     .lyric = TestSupport::fixtureLyric(),
                                                     .language = TestSupport::fixtureLanguage()}
    }));
    const auto settled = [&] {
        return !fixture.clip->pieces().isEmpty() &&
               std::all_of(fixture.clip->pieces().cbegin(), fixture.clip->pieces().cend(),
                           [](const InferPiece *piece) {
                               return piece->state == QStringLiteral("Acoustic.Awaiting") ||
                                      piece->state == QStringLiteral("Ready");
                           }) &&
               taskManager->tasks().isEmpty();
    };
    QTRY_VERIFY_WITH_TIMEOUT(settled(), 15000);
    auto *pitch = fixture.clip->params.getParamByName(ParamInfo::Pitch);
    QVERIFY(pitch && !pitch->curves(Param::Original).isEmpty());
    Automation::CurveDraftDto edited;
    edited.values = QList<int>(385, 6200);
    QVERIFY(runtime.parameters().replaceParameter(fixture.command(), clipId, ParamInfo::Pitch,
                                                  Param::Edited, {edited}));
    QCoreApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(settled(), 15000);
    auto &canvas = *fixture.canvas;
    canvas.setEditMode(ClipEditorGlobal::ModulatePitch);
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto snapshot = [&] {
        QList<DrawCurve> result;
        for (const auto *curve : pitch->curves(Param::Edited))
            result.append(*static_cast<const DrawCurve *>(curve));
        return result;
    };
    const auto initial = snapshot();
    const auto start = fixture.pointFor(600, 60);
    const auto end = fixture.pointFor(840, 60);
    const auto press = fixture.pointFor(720, 60);
    const auto release = press + QPoint(0, 100);
    for (const auto &point : {start, end, press, release})
        QVERIFY(canvas.rect().contains(point));
    const auto selectRange = [&] {
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, start);
        fixture.moveTo(end);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, end);
        for (const auto &boundary : {qMakePair(start, fixture.pointFor(520, 60)),
                                     qMakePair(end, fixture.pointFor(920, 60))}) {
            const auto beforeFrame = fixture.submitted->size();
            QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, boundary.first);
            fixture.moveTo(boundary.second);
            QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, boundary.second);
            fixture.frameAfter(beforeFrame);
            if (QTest::currentTestFailed())
                return;
        }
        QVERIFY(!editSessionManager->hasActiveTransaction());
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(snapshot(), initial);
    };
    selectRange();
    if (QTest::currentTestFailed())
        return;
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, press);
    fixture.moveTo(release);
    QVERIFY(editSessionManager->hasActiveTransaction());
    QCOMPARE(snapshot(), initial);
    QCOMPARE(runtime.documentVersion(), before);
    QTest::keyClick(&canvas, Qt::Key_Escape);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, release);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(snapshot(), initial);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());

    selectRange();
    if (QTest::currentTestFailed())
        return;
    const auto factorHandle = QPoint(press.x(), 20);
    const auto factorRelease = factorHandle + QPoint(0, 100);
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, factorHandle);
    fixture.moveTo(factorRelease);
    const auto committedFrame = fixture.submitted->size();
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, factorRelease);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QTRY_VERIFY_WITH_TIMEOUT(settled(), 15000);
    // Committing pitch also invalidates persisted variance results.
    QVERIFY(runtime.documentVersion().revision > before.revision);
    const auto sampleAt = [&](int tick) -> std::optional<int> {
        for (const auto *curve : pitch->curves(Param::Edited)) {
            const auto *draw = dynamic_cast<const DrawCurve *>(curve);
            if (draw && tick >= draw->localStart() && tick < draw->localEndTick())
                return draw->values().at((tick - draw->localStart()) / draw->step);
        }
        return std::nullopt;
    };
    QCOMPARE(sampleAt(720), std::optional<int>(6000));
    for (int tick : {560, 880})
        QCOMPARE(sampleAt(tick), std::optional<int>(6000));
    for (int tick : {500, 940}) {
        const auto transition = sampleAt(tick);
        QVERIFY2(
            transition && *transition > 6000 && *transition < 6200,
            qPrintable(
                QStringLiteral("Transition at %1: %2").arg(tick).arg(transition.value_or(-1))));
    }
    QCOMPARE(sampleAt(200), std::optional<int>(6200));
    QCOMPARE(sampleAt(1500), std::optional<int>(6200));
    fixture.frameAfter(committedFrame);
    if (QTest::currentTestFailed())
        return;
    historyManager->undo();
    QCOMPARE(snapshot(), initial);
    QVERIFY(!historyManager->canUndo());
    QTRY_VERIFY_WITH_TIMEOUT(settled(), 15000);
    QCOMPARE(snapshot(), initial);
}
