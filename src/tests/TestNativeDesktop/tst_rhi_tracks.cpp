#include "tst_native_desktop.h"
#include "../TestSupport/GuiAppFixture.h"
#include "../TestSupport/WaveFixture.h"

#include "Automation/CoreRuntime.h"
#include "Controller/ClipController.h"
#include "Controller/TrackController.h"
#include "Global/TracksEditorGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/TrackEditor/TracksRhiWidget.h"
#include "UI/Views/TrackEditor/TrackEditorView.h"
#include "UI/Views/TrackEditor/TrackListView.h"
#include "UI/Views/TrackEditor/TracksGraphicsView.h"

#include <lite/History/ActionSequence.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/Tasking/TaskManager.h>

#include <QMouseEvent>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMenu>
#include <QMimeData>
#include <QPointer>
#include <QScopeGuard>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTimer>
#include <QWindow>
#include <QtTest/QTest>

#include <cmath>
#include <numbers>

namespace {
    struct TrackFixture {
        GuiDocumentFixture application;
        std::unique_ptr<QWidget> host;
        QPointer<TracksRhiWidget> canvas;
        int clipId = -1;
        int firstTrackId = -1;
        int secondTrackId = -1;

        ~TrackFixture() {
            if (canvas) {
                QTest::keyClick(canvas.data(), Qt::Key_Escape);
                QTest::mouseRelease(canvas.data(), Qt::LeftButton, Qt::NoModifier,
                                    canvas->rect().center());
            }
            host.reset();
            if (application.context) {
                clipController->setClip(nullptr);
                trackController->setParentWidget(nullptr);
            }
        }

        Automation::CoreRuntime &runtime() const {
            return *application.context->m_coreRuntime;
        }

        Automation::CommandContext command() const {
            return {.expected = runtime().documentVersion(),
                    .source = Automation::InvocationSource::Test};
        }

        bool initialize(const QString &audioPath = {}, bool withEditor = false) {
            if (!application.initialize())
                return false;
            if (withEditor) {
                appOptions->developer()->editorRenderBackend =
                    DeveloperOption::EditorRenderBackend::RhiExperimental;
                host = std::make_unique<TrackEditorView>();
                canvas = host->findChild<TracksRhiWidget *>();
            } else {
                canvas = new TracksRhiWidget;
                host.reset(canvas.data());
            }
            if (!canvas) {
                application.error = QStringLiteral("The RHI track canvas was not created");
                return false;
            }
            canvas->setApi(QRhiWidget::Api::Null);
            auto document = Automation::DocumentAutomationFacade::newDocumentDraft(false);
            Automation::ClipDraftDto clip;
            clip.properties.name = QStringLiteral("Movable phrase");
            clip.properties.start = 480;
            clip.properties.length = 1920;
            clip.properties.clipLen = 960;
            clip.defaultLanguage = QStringLiteral("eng");
            Automation::NoteDraftDto note;
            note.localStart = 120;
            note.length = 480;
            note.keyIndex = 60;
            note.lyric = QStringLiteral("la");
            note.language = QStringLiteral("eng");
            clip.notes = {note};
            if (!audioPath.isEmpty()) {
                clip.type = Automation::ClipDraftDto::Type::Audio;
                clip.notes.clear();
                clip.audioPath = audioPath;
                clip.properties.trimStartMs = 0;
                clip.properties.playLengthMs = 1000;
                clip.properties.materialLengthMs = 1000;
            }
            Automation::TrackDraftDto first;
            first.name = QStringLiteral("Source");
            first.clips.append(clip);
            Automation::TrackDraftDto second;
            second.name = QStringLiteral("Destination");
            document.tracks = {first, second};
            const auto created = runtime().documents().commitNewDocument(command(), document);
            if (!created) {
                application.error = created.getError().message;
                return false;
            }
            const auto tracks = application.context->m_appModel->tracks();
            firstTrackId = tracks[0]->id();
            secondTrackId = tracks[1]->id();
            clipId = (*tracks[0]->clips().begin())->id();
            host->resize(withEditor ? QSize(1200, 500) : QSize(1000, 400));
            host->show();
            host->activateWindow();
            canvas->setFocus();
            canvas->setViewScale(2.0, 1.0);
            canvas->centerAt(1920, 0.5);
            historyManager->reset();
            return true;
        }

        Clip *clip() const {
            return application.context->m_appModel->findClipById(clipId);
        }

        QPoint point(double tick, int trackIndex) const {
            return {qRound((tick - canvas->startTick()) * canvas->width() /
                           (canvas->endTick() - canvas->startTick())),
                    qRound((trackIndex + 0.5) * TracksEditorGlobal::trackHeight * canvas->scaleY() -
                           canvas->logicalVisibleRect().top())};
        }

        int trackOfClip() const {
            int index = -1;
            application.context->m_appModel->findClipById(clipId, index);
            return index;
        }
    };

    void moveWithButton(TracksRhiWidget &canvas, QPoint point,
                        Qt::KeyboardModifiers modifiers = {}) {
        QMouseEvent move(QEvent::MouseMove, QPointF(point), QPointF(canvas.mapToGlobal(point)),
                         Qt::NoButton, Qt::LeftButton, modifiers);
        QApplication::sendEvent(&canvas, &move);
    }
}

void NativeDesktopTests::rhiClipDragScrollsAtTheEdgeAndStopsOnCancel() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    TrackFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.application.error));
    auto &canvas = *fixture.canvas;
    QSignalSpy frames(&canvas, &QRhiWidget::frameSubmitted);
    QSignalSpy failed(&canvas, &QRhiWidget::renderFailed);
    canvas.update();
    QTRY_VERIFY(!frames.isEmpty());
    QTRY_VERIFY(canvas.isActiveWindow());
    const auto oldCursor = QCursor::pos();
    const auto restoreCursor = qScopeGuard([&] { QCursor::setPos(oldCursor); });
    const auto before = fixture.runtime().documentVersion();
    const auto model = fixture.application.context->m_appModel->serialize();
    const auto press = fixture.point(960, 0);
    const auto edge = QPoint(canvas.width() - 2, press.y());
    QVERIFY(canvas.rect().contains(press));
    QVERIFY(canvas.windowHandle());
    const auto release = qScopeGuard(
        [&] { QTest::mouseRelease(canvas.windowHandle(), Qt::LeftButton, Qt::NoModifier, edge); });
    QTest::mousePress(canvas.windowHandle(), Qt::LeftButton, Qt::NoModifier, press);
    QCursor::setPos(canvas.mapToGlobal(edge));
    QTest::mouseMove(canvas.windowHandle(), edge);
    QVERIFY(editSessionManager->hasActiveTransaction());
    const auto afterMove = canvas.startTick();
    const auto previewFrame = frames.size();
    QTRY_VERIFY_WITH_TIMEOUT(canvas.startTick() > afterMove + 60, 3000);
    QTRY_VERIFY(frames.size() > previewFrame);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QCOMPARE(fixture.application.context->m_appModel->serialize(), model);
    QTest::keyClick(&canvas, Qt::Key_Escape);
    QTest::mouseRelease(canvas.windowHandle(), Qt::LeftButton, Qt::NoModifier, edge);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    const auto stoppedAt = canvas.startTick();
    QTest::qWait(80);
    QCOMPARE(canvas.startTick(), stoppedAt);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QCOMPARE(fixture.application.context->m_appModel->serialize(), model);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(failed.isEmpty());
}

void NativeDesktopTests::rhiClipDragCommitsAcrossTracksAndUndoRestoresView() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    QString backendError;
    TrackFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.application.error));
    auto &canvas = *fixture.canvas;
    connect(&canvas, &EditorRhiWidget::backendFailed, &canvas,
            [&](const QString &reason) { backendError = reason; });
    QSignalSpy frames(&canvas, &QRhiWidget::frameSubmitted);
    QSignalSpy failed(&canvas, &QRhiWidget::renderFailed);
    canvas.update();
    QTRY_VERIFY(!frames.isEmpty() || !backendError.isEmpty());
    QVERIFY2(backendError.isEmpty(), qPrintable(backendError));
    QTRY_VERIFY(canvas.isActiveWindow());
    QVERIFY(failed.isEmpty());
    const auto before = fixture.runtime().documentVersion();
    const auto press = fixture.point(960, 0);
    const auto release = fixture.point(1440, 1);
    QVERIFY(canvas.rect().contains(press));
    QVERIFY(canvas.rect().contains(release));
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::AltModifier, press);
    QCOMPARE(appStatus->selectedClips.get(), QList<int>{fixture.clipId});
    const auto previewFrame = frames.size();
    moveWithButton(canvas, release, Qt::AltModifier);
    QVERIFY(editSessionManager->hasActiveTransaction());
    QCOMPARE(fixture.trackOfClip(), 0);
    QCOMPARE(fixture.clip()->start(), 480);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    const auto pageStart = canvas.startTick();
    const auto nextPagePosition = canvas.endTick() + 120;
    canvas.setAutoPageTurn(true);
    canvas.setPlaybackPosition(nextPagePosition);
    QCOMPARE(canvas.startTick(), pageStart);
    canvas.setAutoPageTurn(false);
    QTRY_VERIFY(frames.size() > previewFrame || !backendError.isEmpty());
    QVERIFY2(backendError.isEmpty(), qPrintable(backendError));

    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::AltModifier, release);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(fixture.trackOfClip(), 1);
    QCOMPARE(fixture.clip()->start(), 960);
    QCOMPARE(fixture.runtime().documentVersion().revision, before.revision + 1);
    const auto *entry = historyManager->nextUndoEntry();
    QVERIFY(entry && entry->focusTransition());
    QCOMPARE(canvas.focusVisibility(entry->focusTransition()->after),
             HistoryFocusVisibility::Visible);
    auto frameCount = frames.size();
    QVERIFY(fixture.runtime().history().undo(fixture.command()));
    QCOMPARE(fixture.trackOfClip(), 0);
    QCOMPARE(fixture.clip()->start(), 480);
    QVERIFY(!historyManager->canUndo());
    QTRY_VERIFY(frames.size() > frameCount || !backendError.isEmpty());
    QVERIFY2(backendError.isEmpty(), qPrintable(backendError));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, fixture.point(960, 0));
    QCOMPARE(appStatus->selectedClips.get(), QList<int>{fixture.clipId});

    frameCount = frames.size();
    QVERIFY(fixture.runtime().history().redo(fixture.command()));
    QCOMPARE(fixture.trackOfClip(), 1);
    QCOMPARE(fixture.clip()->start(), 960);
    QTRY_VERIFY(frames.size() > frameCount || !backendError.isEmpty());
    QVERIFY2(backendError.isEmpty(), qPrintable(backendError));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, fixture.point(1440, 1));
    QCOMPARE(appStatus->selectedClips.get(), QList<int>{fixture.clipId});
    QCOMPARE(fixture.runtime().documentVersion().revision, before.revision + 3);
    QVERIFY(failed.isEmpty());
    const auto afterEditing = fixture.runtime().documentVersion();
    canvas.setAutoPageTurn(true);
    QTRY_VERIFY(canvas.startTick() > pageStart && canvas.startTick() <= nextPagePosition &&
                canvas.endTick() >= nextPagePosition);
    canvas.setPlaybackPosition(480);
    QTRY_VERIFY(canvas.startTick() <= 480 && canvas.endTick() >= 480);
    QCOMPARE(fixture.runtime().documentVersion(), afterEditing);
    QCOMPARE(fixture.clip()->start(), 960);
}

void NativeDesktopTests::rhiClipResizeCommitsOrCancels_data() {
    QTest::addColumn<bool>("leftEdge");
    QTest::newRow("extend-right-and-undo") << false;
    QTest::newRow("trim-left-and-cancel") << true;
}

void NativeDesktopTests::rhiClipResizeCommitsOrCancels() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    QFETCH(bool, leftEdge);
    QString backendError;
    TrackFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.application.error));
    auto &canvas = *fixture.canvas;
    connect(&canvas, &EditorRhiWidget::backendFailed, &canvas,
            [&](const QString &reason) { backendError = reason; });
    QSignalSpy frames(&canvas, &QRhiWidget::frameSubmitted);
    QSignalSpy failed(&canvas, &QRhiWidget::renderFailed);
    canvas.update();
    QTRY_VERIFY(!frames.isEmpty() || !backendError.isEmpty());
    QVERIFY2(backendError.isEmpty(), qPrintable(backendError));
    QTRY_VERIFY(canvas.isActiveWindow());
    const auto before = fixture.runtime().documentVersion();
    const auto press = fixture.point(leftEdge ? 481 : 1439, 0);
    const auto release = fixture.point(leftEdge ? 721 : 1679, 0);
    QVERIFY(canvas.rect().contains(press));
    QVERIFY(canvas.rect().contains(release));
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::AltModifier, press);
    moveWithButton(canvas, release, Qt::AltModifier);
    QVERIFY(editSessionManager->hasActiveTransaction());
    QCOMPARE(fixture.clip()->clipStart(), 0);
    QCOMPARE(fixture.clip()->clipLen(), 960);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    if (leftEdge) {
        QTest::keyClick(&canvas, Qt::Key_Escape);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::AltModifier, release);
        QCOMPARE(fixture.runtime().documentVersion(), before);
        QVERIFY(!historyManager->canUndo());
    } else {
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::AltModifier, release);
        QCOMPARE(fixture.clip()->clipLen(), 1200);
        QCOMPARE(fixture.runtime().documentVersion().revision, before.revision + 1);
        QVERIFY(fixture.runtime().history().undo(fixture.command()));
        QVERIFY(!historyManager->canUndo());
    }
    QCOMPARE(fixture.clip()->start(), 480);
    QCOMPARE(fixture.clip()->clipStart(), 0);
    QCOMPARE(fixture.clip()->clipLen(), 960);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QVERIFY(failed.isEmpty());
}

void NativeDesktopTests::rhiTrackMenuPasteAndSelectionUseTheFullEditor() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    TrackFixture fixture;
    QVERIFY2(fixture.initialize({}, true), qPrintable(fixture.application.error));
    auto &canvas = *fixture.canvas;
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
    QSignalSpy frames(&canvas, &QRhiWidget::frameSubmitted);
    QSignalSpy failed(&canvas, &QRhiWidget::renderFailed);
    canvas.update();
    QTRY_VERIFY(!frames.isEmpty());
    QVERIFY(failed.isEmpty());
    QTRY_VERIFY(fixture.host->isActiveWindow());
    const auto before = fixture.runtime().documentVersion();
    auto *destination = fixture.application.context->m_appModel->tracks().last();
    TrackEditorMenuContext requested;
    connect(&canvas, &TracksRhiWidget::contextMenuRequested, &canvas,
            [&](const TrackEditorMenuContext &context) { requested = context; });
    const auto menu = [&](QPoint position, const QString &text, bool commit, bool preview) {
        bool entered = false;
        QTimer respond;
        respond.setSingleShot(true);
        connect(&respond, &QTimer::timeout, &canvas, [&] {
            auto *popup = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(popup);
            const auto close = qScopeGuard([&] { popup->close(); });
            entered = true;
            QAction *action = nullptr;
            for (auto *candidate : popup->actions()) {
                if (candidate->text() == text)
                    action = candidate;
            }
            QVERIFY(action && action->isEnabled());
            const auto target = popup->actionGeometry(action).center();
            if (preview) {
                const auto rendered = frames.size();
                QTest::mouseMove(popup->windowHandle(), target);
                QTRY_COMPARE(popup->activeAction(), action);
                QTRY_VERIFY(frames.size() > rendered);
                QCOMPARE(destination->clips().count(), 0);
                QCOMPARE(fixture.runtime().documentVersion(), before);
                QVERIFY(!historyManager->canUndo());
            }
            if (commit)
                QTest::mouseClick(popup, Qt::LeftButton, Qt::NoModifier, target);
            else
                QTest::keyClick(popup, Qt::Key_Escape);
        });
        const auto global = canvas.mapToGlobal(position);
        QTest::mouseMove(fixture.host->windowHandle(), fixture.host->mapFromGlobal(global));
        QContextMenuEvent event(QContextMenuEvent::Mouse, position, global);
        respond.start(0);
        QApplication::sendEvent(&canvas, &event);
        QVERIFY(entered);
    };
    menu(fixture.point(960, 0), TrackEditorContextMenuController::tr("&Copy"), true, false);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(requested.target, TrackEditorMenuContext::Target::SingingClip);
    QCOMPARE(requested.clipId, fixture.clipId);
    for (bool commit : {false, true}) {
        menu(fixture.point(2180, 1), TrackEditorContextMenuController::tr("&Paste"), commit, true);
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(requested.target, TrackEditorMenuContext::Target::Background);
        QCOMPARE(requested.trackIndex, 1);
        QCOMPARE(destination->clips().count(), commit ? 1 : 0);
    }
    auto *pasted = dynamic_cast<SingingClip *>(*destination->clips().begin());
    QVERIFY(pasted);
    QCOMPARE(pasted->start(), requested.snappedTick);
    QCOMPARE(pasted->clipLen(), fixture.clip()->clipLen());
    QCOMPARE(pasted->notes().count(), 1);
    QCOMPARE((*pasted->notes().begin())->lyric(), QStringLiteral("la"));
    const auto pastedId = pasted->id();
    const auto *pasteUndo = historyManager->nextUndoEntry();
    QVERIFY(pasteUndo && pasteUndo->focusTransition());
    canvas.setSceneLength(64000);
    QVERIFY(canvas.centerAt(40000, 0));
    const auto focus = pasteUndo->focusTransition()->after;
    QCOMPARE(canvas.focusVisibility(focus), HistoryFocusVisibility::ScrollRequired);
    QVERIFY(canvas.revealFocus(focus, false));
    QCOMPARE(canvas.focusVisibility(focus), HistoryFocusVisibility::Visible);
    QVERIFY(canvas.centerAt(1920, 0.5));
    const auto selectFrom = fixture.point(240, 0);
    const auto selectTo = fixture.point(3300, 1);
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, selectFrom);
    moveWithButton(canvas, selectTo);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, selectTo);
    const auto selection = appStatus->selectedClips.get();
    QCOMPARE(selection.size(), 2);
    QVERIFY(selection.contains(fixture.clipId) && selection.contains(pastedId));
    QCOMPARE(historyManager->nextUndoEntry(), pasteUndo);
    historyManager->undo();
    QCOMPARE(destination->clips().count(), 0);
    QVERIFY(!fixture.application.context->m_appModel->findClipById(pastedId));
    QVERIFY(!historyManager->canUndo());
    const auto newPosition = fixture.point(2400, 1);
    QTest::mouseDClick(&canvas, Qt::LeftButton, Qt::NoModifier, newPosition);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, newPosition);
    QCOMPARE(destination->clips().count(), 1);
    const auto *created = dynamic_cast<SingingClip *>(*destination->clips().begin());
    QVERIFY(created);
    QCOMPARE(created->start(), 2400);
    QCOMPARE(created->notes().count(), 0);
    historyManager->undo();
    QCOMPARE(destination->clips().count(), 0);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(failed.isEmpty());

    auto *editor = qobject_cast<TrackEditorView *>(fixture.host.get());
    QVERIFY(editor);
    QVERIFY(editor->setViewScale(2, 4));
    QVERIFY(editor->centerAt(1920, 0.5));
    const auto viewport = editor->viewState();
    const auto beforeFallback = fixture.runtime().documentVersion();
    const auto *source = fixture.application.context->m_appModel->tracks().first();
    const auto sourceTrack = source->serialize();
    canvas.backendFailed(QStringLiteral("Rendering device lost"));
    QTRY_VERIFY(fixture.canvas.isNull());
    auto *legacy = editor->findChild<TracksGraphicsView *>();
    auto *list = editor->findChild<TrackListView *>();
    QVERIFY(legacy && list);
    QTRY_VERIFY(legacy->isVisible());
    QTRY_COMPARE(editor->viewState().horizontalScale, viewport.horizontalScale);
    QCOMPARE(editor->viewState().verticalScale, viewport.verticalScale);
    auto *scroll = list->verticalScrollBar();
    QTRY_VERIFY(scroll->maximum() > scroll->minimum());
    const auto initialScroll = scroll->value();
    QTest::keyClick(scroll, Qt::Key_Down);
    QTRY_VERIFY(scroll->value() > initialScroll);
    QCOMPARE(legacy->verticalScrollBar()->value(), scroll->value());
    QCOMPARE(fixture.runtime().documentVersion(), beforeFallback);
    QCOMPARE(source->serialize(), sourceTrack);
    QVERIFY(editor->setViewScale(2, 1));
    QVERIFY(editor->centerAt(1920, 0.5));
    QCoreApplication::processEvents();
    const auto legacyPosition = legacy->mapFromScene(QPointF(
        legacy->sceneXForTick(2400), 1.5 * TracksEditorGlobal::trackHeight * legacy->scaleY()));
    QVERIFY(legacy->viewport()->rect().contains(legacyPosition));
    QTest::mouseDClick(legacy->viewport(), Qt::LeftButton, Qt::NoModifier, legacyPosition);
    QTest::mouseRelease(legacy->viewport(), Qt::LeftButton, Qt::NoModifier, legacyPosition);
    QCOMPARE(destination->clips().count(), 1);
    QCOMPARE((*destination->clips().begin())->start(), 2400);
    QCOMPARE(source->serialize(), sourceTrack);
    historyManager->undo();
    QCOMPARE(destination->clips().count(), 0);
    QVERIFY(!historyManager->canUndo());
}

void NativeDesktopTests::rhiTrackFileDropImportsAtTheChosenSlot_data() {
    QTest::addColumn<bool>("append");
    QTest::newRow("existing-track") << false;
    QTest::newRow("append-track") << true;
}

void NativeDesktopTests::rhiTrackFileDropImportsAtTheChosenSlot() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    QFETCH(bool, append);
    TrackFixture fixture;
    QVERIFY2(fixture.initialize({}, true), qPrintable(fixture.application.error));
    auto &canvas = *fixture.canvas;
    const auto path = fixture.application.directory.filePath(QStringLiteral("drop.wav"));
    QVERIFY(TestSupport::writeWave(path, QVector<float>(4800, 0.125f)));
    QSignalSpy frames(&canvas, &QRhiWidget::frameSubmitted);
    QSignalSpy failed(&canvas, &QRhiWidget::renderFailed);
    canvas.update();
    QTRY_VERIFY(!frames.isEmpty());
    QTRY_VERIFY(fixture.host->isActiveWindow());
    auto *list = fixture.host->findChild<TrackListView *>();
    QVERIFY(list);
    QCOMPARE(list->trackCount(), 2);
    auto *model = fixture.application.context->m_appModel;
    const auto tracksBefore = model->tracks();
    const auto before = fixture.runtime().documentVersion();
    const auto position = fixture.point(960, append ? 2 : 1);
    QVERIFY(canvas.rect().contains(position));
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(path)});
    QDragEnterEvent preview(position, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &preview);
    QVERIFY(preview.isAccepted());
    const auto previewFrame = frames.size();
    QTRY_VERIFY(frames.size() > previewFrame);
    QCOMPARE(model->tracks(), tracksBefore);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QDragLeaveEvent leave;
    QApplication::sendEvent(&canvas, &leave);
    QVERIFY(leave.isAccepted());
    QVERIFY(!historyManager->canUndo());
    QDragEnterEvent enter(position, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &enter);
    QVERIFY(enter.isAccepted());
    QDragMoveEvent move(position, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &move);
    QVERIFY(move.isAccepted());
    QDropEvent drop(position, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &drop);
    QVERIFY(drop.isAccepted());
    QTRY_COMPARE(model->tracks().size(), append ? 3 : 2);
    QTRY_COMPARE(model->tracks().last()->clips().count(), 1);
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    auto *audio = dynamic_cast<AudioClip *>(*model->tracks().last()->clips().begin());
    QVERIFY(audio);
    QCOMPARE(audio->audioInfo().frames, 4800);
    QCOMPARE(audio->start() + audio->clipStart(), 960);
    QCOMPARE(audio->playLengthMs(), 100.0);
    QTRY_COMPARE(list->trackCount(), append ? 3 : 2);
    const auto audioId = audio->id();
    historyManager->undo();
    QCOMPARE(model->tracks(), tracksBefore);
    QCOMPARE(model->tracks().last()->clips().count(), 0);
    QTRY_COMPARE(list->trackCount(), 2);
    QVERIFY(!model->findClipById(audioId));
    QVERIFY(!historyManager->canUndo());
    QVERIFY(failed.isEmpty());
}

void NativeDesktopTests::rhiFileDropScrollsUntilTheDragLeaves() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    TrackFixture fixture;
    QVERIFY2(fixture.initialize({}, true), qPrintable(fixture.application.error));
    auto &canvas = *fixture.canvas;
    TestSupport::placeWindowOnScreen(*fixture.host, {1200, 500});
    QTRY_VERIFY(fixture.host->isActiveWindow());
    QSignalSpy frames(&canvas, &QRhiWidget::frameSubmitted);
    QSignalSpy failed(&canvas, &QRhiWidget::renderFailed);
    QVERIFY(canvas.setViewScale(2, 4));
    QVERIFY(canvas.centerAt(1920, 1));
    canvas.update();
    QTRY_VERIFY(!frames.isEmpty());
    const auto path = fixture.application.directory.filePath(QStringLiteral("scroll-drop.wav"));
    QVERIFY(TestSupport::writeWave(path, QVector<float>(4800, 0.125f)));
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(path)});
    const auto start = fixture.point(960, 1);
    const auto edge = QPoint(start.x(), canvas.height() - 2);
    QVERIFY(canvas.rect().contains(start));
    auto *window = fixture.host->windowHandle();
    QVERIFY(window);
    const auto oldCursor = QCursor::pos();
    const auto release = qScopeGuard([&] {
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier,
                            window->mapFromGlobal(canvas.mapToGlobal(edge)));
        QCursor::setPos(oldCursor);
    });
    QCursor::setPos(canvas.mapToGlobal(edge));
    QCoreApplication::processEvents();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier,
                      window->mapFromGlobal(canvas.mapToGlobal(start)));
    QVERIFY(QGuiApplication::mouseButtons().testFlag(Qt::LeftButton));
    const auto model = fixture.application.context->m_appModel->serialize();
    const auto before = fixture.runtime().documentVersion();
    QDragEnterEvent enter(start, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &enter);
    QVERIFY(enter.isAccepted());
    QDragMoveEvent move(edge, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &move);
    QVERIFY(move.isAccepted());
    const auto initialOffset = canvas.logicalVisibleRect().top();
    QTRY_VERIFY_WITH_TIMEOUT(canvas.logicalVisibleRect().top() > initialOffset + 4, 3000);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QCOMPARE(fixture.application.context->m_appModel->serialize(), model);
    QDragLeaveEvent leave;
    QApplication::sendEvent(&canvas, &leave);
    QVERIFY(leave.isAccepted());
    const auto stoppedOffset = canvas.logicalVisibleRect().top();
    QTest::qWait(80);
    QCOMPARE(canvas.logicalVisibleRect().top(), stoppedOffset);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(failed.isEmpty());
}

void NativeDesktopTests::rhiAudioClipTrimAndMovePreserveTimeAnchors() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    TrackFixture fixture;
    QVERIFY(fixture.application.directory.isValid());
    const auto path = fixture.application.directory.filePath(QStringLiteral("stereo.wav"));
    QVector<float> samples(48000 * 2);
    for (int frame = 0; frame < 48000; ++frame) {
        const auto phase = 2.0 * std::numbers::pi * 440 * frame / 48000;
        const auto amplitude = frame < 4800 || frame >= 43200 ? 0.0f : 1.0f;
        samples[frame * 2] = amplitude * 0.2f * static_cast<float>(std::sin(phase));
        samples[frame * 2 + 1] = amplitude * 0.1f * static_cast<float>(std::cos(phase));
    }
    QVERIFY(TestSupport::writeWave(path, samples, 2));
    QVERIFY2(fixture.initialize(path), qPrintable(fixture.application.error));
    auto &canvas = *fixture.canvas;
    auto *audio = dynamic_cast<AudioClip *>(fixture.clip());
    QVERIFY(audio);
    QTRY_VERIFY(audio->audioInfo().frames == 48000 && !audio->audioInfo().peakCache.isEmpty() &&
                taskManager->tasks().isEmpty());
    QVERIFY(audio->hasRealTimeAnchor());
    QCOMPARE(audio->trimStartMs(), 0.0);
    QCOMPARE(audio->playLengthMs(), 1000.0);
    QCOMPARE(audio->materialLengthMs(), 1000.0);
    QCOMPARE(audio->start() + audio->clipStart(), 480);
    QCOMPARE(audio->clipLen(), 960);
    QString backendError;
    connect(&canvas, &EditorRhiWidget::backendFailed, &canvas,
            [&](const QString &reason) { backendError = reason; });
    QSignalSpy frames(&canvas, &QRhiWidget::frameSubmitted);
    QSignalSpy failed(&canvas, &QRhiWidget::renderFailed);
    canvas.update();
    QTRY_VERIFY(!frames.isEmpty() || !backendError.isEmpty());
    QVERIFY2(backendError.isEmpty(), qPrintable(backendError));
    QTRY_VERIFY(canvas.isActiveWindow());
    historyManager->reset();
    const auto modelBeforeZoom = fixture.application.context->m_appModel->serialize();
    const auto versionBeforeZoom = fixture.runtime().documentVersion();
    // The overview, individual peaks, and samples keep the same timeline hit target.
    for (const double scale : {2.0, 16.0, 512.0}) {
        const auto rendered = frames.size();
        QVERIFY(canvas.setViewScale(scale, 1));
        QVERIFY(canvas.centerAt(960, 0));
        canvas.update();
        QTRY_VERIFY(frames.size() > rendered || !backendError.isEmpty());
        QVERIFY2(backendError.isEmpty(), qPrintable(backendError));
        QVERIFY(canvas.startTick() < 960 && canvas.endTick() > 960);
        appStatus->selectedClips = {};
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, fixture.point(960, 0));
        QCOMPARE(appStatus->selectedClips.get(), QList<int>{fixture.clipId});
        QCOMPARE(fixture.application.context->m_appModel->serialize(), modelBeforeZoom);
        QCOMPARE(fixture.runtime().documentVersion(), versionBeforeZoom);
        QVERIFY(!historyManager->canUndo());
    }
    const auto restoredFrame = frames.size();
    QVERIFY(canvas.setViewScale(2, 1));
    QVERIFY(canvas.centerAt(1920, 0.5));
    QTRY_VERIFY(frames.size() > restoredFrame);
    const auto before = fixture.runtime().documentVersion();
    const auto press = fixture.point(481, 0);
    const auto release = fixture.point(721, 0);
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::AltModifier, press);
    moveWithButton(canvas, release, Qt::AltModifier);
    QVERIFY(editSessionManager->hasActiveTransaction());
    QCOMPARE(audio->trimStartMs(), 0.0);
    QCOMPARE(audio->playLengthMs(), 1000.0);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::AltModifier, release);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(audio->trimStartMs(), 250.0);
    QCOMPARE(audio->playLengthMs(), 750.0);
    QCOMPARE(audio->materialLengthMs(), 1000.0);
    QCOMPARE(audio->start() + audio->clipStart(), 720);
    QCOMPARE(audio->start() + audio->clipStart() + audio->clipLen(), 1440);
    QCOMPARE(fixture.runtime().documentVersion().revision, before.revision + 1);
    historyManager->undo();
    audio = dynamic_cast<AudioClip *>(fixture.clip());
    QVERIFY(audio);
    QCOMPARE(audio->trimStartMs(), 0.0);
    QCOMPARE(audio->playLengthMs(), 1000.0);
    QCOMPARE(audio->start() + audio->clipStart(), 480);
    QVERIFY(!historyManager->canUndo());

    const auto moveFrom = fixture.point(960, 0);
    const auto moveTo = fixture.point(1440, 1);
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, moveFrom);
    moveWithButton(canvas, moveTo);
    const auto beforeMoveFrame = frames.size();
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, moveTo);
    QCOMPARE(fixture.trackOfClip(), 1);
    audio = dynamic_cast<AudioClip *>(fixture.clip());
    QVERIFY(audio);
    QCOMPARE(audio->start() + audio->clipStart(), 960);
    QCOMPARE(audio->trimStartMs(), 0.0);
    QCOMPARE(audio->playLengthMs(), 1000.0);
    QCOMPARE(audio->materialLengthMs(), 1000.0);
    QCOMPARE(audio->path(), path);
    QTRY_VERIFY(frames.size() > beforeMoveFrame || !backendError.isEmpty());
    QVERIFY2(backendError.isEmpty(), qPrintable(backendError));
    const auto beforeUndoFrame = frames.size();
    historyManager->undo();
    QCOMPARE(fixture.trackOfClip(), 0);
    QCOMPARE(fixture.clip()->start() + fixture.clip()->clipStart(), 480);
    QVERIFY(!historyManager->canUndo());
    QTRY_VERIFY(frames.size() > beforeUndoFrame || !backendError.isEmpty());
    QVERIFY2(backendError.isEmpty(), qPrintable(backendError));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, fixture.point(960, 0));
    QCOMPARE(appStatus->selectedClips.get(), QList<int>{fixture.clipId});
    QVERIFY(failed.isEmpty());
}
