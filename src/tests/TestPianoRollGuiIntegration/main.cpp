#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Bootstrap/AppDataPaths.h"
#include "Bootstrap/AppEnvironment.h"
#include "Controller/ClipController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
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

#include <QtTest/QTest>
#include <QApplication>
#include <QDir>
#include <QMouseEvent>
#include <QTemporaryDir>

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
        // This gesture does not use playback; an absent device must not prevent editing.
        options->audio()->obj.insert(QStringLiteral("driverName"),
                                     QStringLiteral("gui-test-no-audio-driver"));
        options->audio()->obj.insert(QStringLiteral("deviceName"),
                                     QStringLiteral("gui-test-no-audio-device"));
        context = std::make_unique<AppContext>(std::move(options), AppHostMode::Gui);
        QVERIFY(QApplication::activeModalWidget() == nullptr);
        QVERIFY2(ThemeManager::instance()->initialize(ThemeIds::defaultThemeId()),
                 qPrintable(ThemeLoader::lastError()));
        QString error;
        QVERIFY2(context->initializeDefaultDocument(&error), qPrintable(error));

        auto &runtime = *context->m_coreRuntime;
        Automation::TrackDraftDto track;
        track.name = QStringLiteral("Mouse gesture");
        track.defaultLanguage = QStringLiteral("eng");
        const auto insertedTrack = runtime.project().insertTrack(commandContext(), 0, track);
        QVERIFY(insertedTrack);
        QCOMPARE(insertedTrack.get().affectedObjects.size(), 1);
        const Automation::TrackId trackId(insertedTrack.get().affectedObjects.first().value);

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

    void cleanupTestCase() {
        if (view) {
            view->setDataContext(nullptr);
            view.reset();
        }
        scene.reset();
        if (context) {
            clipController->setClip(nullptr);
            singingClip = nullptr;
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

    QTemporaryDir dataRoot;
    QByteArray previousDataRoot;
    bool dataRootInstalled = false;
    std::unique_ptr<AppContext> context;
    std::unique_ptr<PianoRollGraphicsScene> scene;
    std::unique_ptr<PianoRollGraphicsView> view;
    SingingClip *singingClip = nullptr;
};

QTEST_MAIN(PianoRollGuiIntegrationTests)
#include "main.moc"
