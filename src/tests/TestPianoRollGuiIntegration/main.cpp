#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Bootstrap/AppDataPaths.h"
#include "Bootstrap/AppEnvironment.h"
#include "Controller/ClipController.h"
#include "Controller/ClipboardController.h"
#include "Controller/PlaybackController.h"
#include "Controller/TrackController.h"
#include "Global/ControllerGlobal.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/OutputSystem.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/ClipEditor/PianoRoll/NoteView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollCoord.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsScene.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"

#include <lite/GUI/Theme/ThemeIds.h>
#include <lite/GUI/Theme/ThemeLoader.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <TalcsDevice/AudioDevice.h>

#include <QtTest/QTest>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDialog>
#include <QMouseEvent>
#include <QMimeData>
#include <QTemporaryDir>
#include <QTimer>

#include <memory>

class PianoRollGuiIntegrationTests final : public QObject {
    Q_OBJECT

private slots:

    void initTestCase() {
        QVERIFY(dataRoot.isValid());
        previousDataRoot = qgetenv("DSEL_TEST_DATA_ROOT");
        qputenv("DSEL_TEST_DATA_ROOT", dataRoot.path().toUtf8());
        dataRootInstalled = true;
        QCOMPARE(AppDataPaths::testRoot(), QDir::cleanPath(dataRoot.path()));
        QApplication::setQuitOnLastWindowClosed(false);
        AppEnvironment::postInit(AppHostMode::Gui);

        auto options = std::make_unique<AppOptions>();
        QVERIFY(QDir::cleanPath(options->configPath()).startsWith(dataRoot.path() + '/'));
        options->general()->packageSearchPaths.clear();
        options->general()->defaultSingingLanguage = QStringLiteral("eng");
        options->inference()->autoStartInfer = false;
        options->inference()->executionProvider = QStringLiteral("CPU");
        options->inference()->cacheDirectory = dataRoot.filePath(QStringLiteral("cache"));
        options->appearance()->animationEnabled = false;
        context = std::make_unique<AppContext>(std::move(options), AppHostMode::Gui);
        // Close the fixture-owned stream so playback failure is independent of host devices.
        if (auto *device = AudioSystem::outputSystem()->context()->device()) {
            device->stop();
            device->close();
            QVERIFY(!device->isOpen());
            QVERIFY(!device->isStarted());
        }
        QVERIFY(QApplication::activeModalWidget() == nullptr);
        QVERIFY2(ThemeManager::instance()->initialize(ThemeIds::defaultThemeId()),
                 qPrintable(ThemeLoader::lastError()));
        savedClipboard = std::make_unique<QMimeData>();
        if (const auto *mime = QApplication::clipboard()->mimeData()) {
            for (const auto &format : mime->formats())
                savedClipboard->setData(format, mime->data(format));
        }
    }

    void init() {
        QString error;
        QVERIFY2(context->initializeDefaultDocument(&error), qPrintable(error));

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
        const auto insertedClip = runtime.project().insertClips(
            commandContext(), {
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

    void copyPasteUsesTheActiveClipAndPlaybackPosition() {
        auto &runtime = *context->m_coreRuntime;
        const auto sourceId = insertSelectedNote();
        QVERIFY(sourceId >= 0);
        Automation::ClipDraftDto clip;
        clip.type = Automation::ClipDraftDto::Type::Singing;
        clip.properties.length = 3840;
        clip.properties.clipLen = 3840;
        clip.defaultLanguage = QStringLiteral("eng");
        const auto inserted = runtime.project().insertClips(
            commandContext(), {
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

    void cutCopiesThenRemovesSelectionAsOneUndoStep() {
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

    void wholeClipClipboardUsesSelectedTrackAndPreservesCurves() {
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
        const auto parameter = runtime.parameters().getParameter(
            runtime.documentVersion().documentId, Automation::ClipId(pasted->id()),
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

    void publicPlaybackDeviceFailureDoesNotOpenAModalDialog() {
        auto &runtime = *context->m_coreRuntime;
        const auto *device = AudioSystem::outputSystem()->context()->device();
        QVERIFY(!device || !device->isOpen());
        const auto before = runtime.playback().getPlayback(runtime.documentVersion().documentId);
        QVERIFY(before);
        QVERIFY(before.get().playable);
        QCOMPARE(before.get().state, Automation::PlaybackState::Stopped);
        bool dialogShown = false;
        QTimer dismissUnexpectedDialog;
        connect(&dismissUnexpectedDialog, &QTimer::timeout, this, [&] {
            if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
                dialogShown = true;
                dialog->reject();
            }
        });
        dismissUnexpectedDialog.start(10);
        auto command = commandContext();
        command.source = Automation::InvocationSource::PublicMcp;
        const auto result = runtime.playback().play(command);
        dismissUnexpectedDialog.stop();
        QVERIFY(!dialogShown);
        QVERIFY(!result);
        QCOMPARE(result.getError().code,
                 Automation::AutomationErrorCode::HostCapabilityUnavailable);
        const auto after = runtime.playback().getPlayback(runtime.documentVersion().documentId);
        QVERIFY(after);
        QCOMPARE(after.get().state, before.get().state);
        QCOMPARE(after.get().position, before.get().position);
        QCOMPARE(after.get().lastPosition, before.get().lastPosition);
        QCOMPARE(after.get().document, before.get().document);
        QVERIFY(!historyManager->canUndo());
    }

    void invalidClipboardDoesNotEdit_data() {
        QTest::addColumn<QString>("format");
        QTest::addColumn<QByteArray>("bytes");
        const auto notes = ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams);
        QTest::newRow("unrelated-text") << QStringLiteral("text/plain") << QByteArray("hello");
        QTest::newRow("malformed-note-json") << notes << QByteArray("{broken");
        QTest::newRow("empty-note-payload") << notes << QByteArray("{}");
    }

    void invalidClipboardDoesNotEdit() {
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

    void drawingCommitsOnceAndUndoRedoUpdatesTheScene() {
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
                         QPointF(view->viewport()->mapToGlobal(release)), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
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

    void draggingExistingNoteCommitsOrCancels_data() {
        QTest::addColumn<bool>("cancel");
        QTest::newRow("release-commits") << false;
        QTest::newRow("escape-cancels") << true;
    }

    void draggingExistingNoteCommitsOrCancels() {
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
                         QPointF(view->viewport()->mapToGlobal(release)), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
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

    void cleanup() {
        if (view) {
            view->setDataContext(nullptr);
            view.reset();
        }
        scene.reset();
        clipController->setClip(nullptr);
        singingClip = nullptr;
    }

    void cleanupTestCase() {
        if (savedClipboard)
            QApplication::clipboard()->setMimeData(savedClipboard.release());
        if (context) {
            context.reset();
        }
        if (dataRootInstalled) {
            if (previousDataRoot.isEmpty())
                qunsetenv("DSEL_TEST_DATA_ROOT");
            else
                qputenv("DSEL_TEST_DATA_ROOT", previousDataRoot);
        }
    }

private:
    int insertSelectedNote() {
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

    Automation::CommandContext commandContext() const {
        return {.expected = context->m_coreRuntime->documentVersion(),
                .source = Automation::InvocationSource::Test};
    }

    QPoint pointFor(int tick, int key) const {
        return view->mapFromScene(QPointF(
            view->tickToSceneX(tick),
            PianoRollCoord::keyIndexToCenterY(key, ClipEditorGlobal::noteHeight * view->scaleY())));
    }

    int sceneNoteCount(int id) const {
        int count = 0;
        for (const auto *item : scene->items()) {
            const auto *note = dynamic_cast<const NoteView *>(item);
            if (note && note->id() == id)
                ++count;
        }
        return count;
    }

    const NoteView *sceneNote(int id) const {
        for (const auto *item : scene->items()) {
            const auto *note = dynamic_cast<const NoteView *>(item);
            if (note && note->id() == id)
                return note;
        }
        return nullptr;
    }

    QTemporaryDir dataRoot;
    QByteArray previousDataRoot;
    bool dataRootInstalled = false;
    std::unique_ptr<AppContext> context;
    std::unique_ptr<PianoRollGraphicsScene> scene;
    std::unique_ptr<PianoRollGraphicsView> view;
    SingingClip *singingClip = nullptr;
    Automation::TrackId trackId;
    std::unique_ptr<QMimeData> savedClipboard;
};

QTEST_MAIN(PianoRollGuiIntegrationTests)
#include "main.moc"
