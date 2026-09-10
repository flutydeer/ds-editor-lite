#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/TrackController.h"
#include "Global/ControllerGlobal.h"
#include "Global/TracksEditorGlobal.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Views/TrackEditor/GraphicsItem/AbstractClipView.h"
#include "UI/Views/TrackEditor/TrackControlView.h"
#include "UI/Views/TrackEditor/TrackEditorContextMenuController.h"
#include "UI/Views/TrackEditor/TrackEditorView.h"
#include "UI/Views/TrackEditor/TracksGraphicsView.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/Tasking/TaskManager.h>

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPointer>
#include <QPushButton>
#include <QScopeGuard>
#include <QTimer>
#include <QWindow>
#include <QtTest/QTest>

namespace {
    TrackControlView *trackControls(TrackEditorView &editor, int id) {
        for (auto *controls : editor.findChildren<TrackControlView *>()) {
            if (controls->id() == id)
                return controls;
        }
        return nullptr;
    }

    void chooseTrackMenu(QWidget *surface, const QPoint &position, const QString &text) {
        QVERIFY(surface);
        QVERIFY(surface->rect().contains(position));
        bool entered = false;
        QTimer action;
        action.setSingleShot(true);
        QObject::connect(&action, &QTimer::timeout, surface, [&] {
            QPointer<QMenu> menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(menu);
            const auto closeMenu = qScopeGuard([&] {
                if (menu)
                    menu->close();
            });
            entered = true;
            QAction *selected = nullptr;
            for (auto *candidate : menu->actions()) {
                if (candidate->text() == text)
                    selected = candidate;
            }
            QVERIFY2(selected, qPrintable(text));
            QVERIFY(selected->isEnabled());
            QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                              menu->actionGeometry(selected).center());
        });
        const auto previousCursor = QCursor::pos();
        const auto restoreCursor = qScopeGuard([&] { QCursor::setPos(previousCursor); });
        const auto global = surface->mapToGlobal(position);
        QCursor::setPos(global);
        QTest::mouseMove(surface->window()->windowHandle(),
                         surface->window()->mapFromGlobal(global));
        QContextMenuEvent event(QContextMenuEvent::Mouse, position, global);
        action.start(0);
        QApplication::sendEvent(surface, &event);
        action.stop();
        QVERIFY(entered);
    }

    void showTrackEditor(TrackEditorView &editor, TracksGraphicsView *canvas) {
        QVERIFY(canvas);
        editor.resize(1200, 550);
        canvas->setAnimationEnabled(false);
        editor.show();
        editor.activateWindow();
        canvas->setFocus();
        QTRY_VERIFY(editor.isActiveWindow() && canvas->viewport()->width() > 600);
        QVERIFY(canvas->setViewportScale(2, 1));
        canvas->setViewportStartTick(0);
        QCoreApplication::processEvents();
    }
}

void ApplicationGuiTests::trackMenusCreateCutAndDeleteWithUndo() {
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    TrackEditorView editor;
    const auto clearParent = qScopeGuard([] { trackController->setParentWidget(nullptr); });
    Automation::TrackDraftDto draft;
    draft.name = QStringLiteral("Existing track");
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, draft));
    const auto existingId = context->m_appModel->tracks().first()->id();
    auto *canvas = editor.findChild<TracksGraphicsView *>();
    showTrackEditor(editor, canvas);
    if (QTest::currentTestFailed())
        return;
    historyManager->reset();
    const auto before = runtime.documentVersion();
    auto *controls = trackControls(editor, existingId);
    QVERIFY(controls);
    chooseTrackMenu(controls, controls->rect().center(), TrackControlView::tr("Insert new track"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(context->m_appModel->tracks().size(), 2);
    auto *created = context->m_appModel->tracks().at(1);
    const auto trackId = created->id();
    QVERIFY(trackId != existingId);
    QCOMPARE(created->defaultLanguage(), appOptions->general()->defaultSingingLanguage);
    QVERIFY(trackControls(editor, trackId));
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);

    constexpr int start = 960;
    const auto position = canvas->mapFromScene(
        QPointF(canvas->sceneXForTick(start), TracksEditorGlobal::trackHeight * 1.5));
    chooseTrackMenu(canvas->viewport(), position,
                    TrackEditorContextMenuController::tr("New singing clip"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(created->clips().count(), 1);
    auto *clip = dynamic_cast<SingingClip *>(*created->clips().begin());
    QVERIFY(clip);
    const auto clipId = clip->id();
    QCOMPARE(clip->start(), start);
    QCOMPARE(clip->length(), 7680);
    QCOMPARE(clip->clipLen(), clip->length());
    QCOMPARE(clip->defaultLanguage(), created->defaultLanguage());
    QCOMPARE(appStatus->activeClipId.get(), clipId);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 2);
    auto *item = editor.findClipItemById(clipId);
    QVERIFY(item);
    const auto clipPosition = canvas->mapFromScene(item->sceneBoundingRect().center());
    QVERIFY(canvas->viewport()->rect().contains(clipPosition));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, clipPosition);
    chooseTrackMenu(canvas->viewport(), clipPosition, TrackEditorContextMenuController::tr("Cu&t"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(created->clips().count(), 0);
    QVERIFY(!editor.findClipItemById(clipId));
    QCOMPARE(appStatus->activeClipId.get(), -1);
    QVERIFY(QApplication::clipboard()->mimeData()->hasFormat(
        ControllerGlobal::ElemMimeType.at(ControllerGlobal::Clip)));
    QCOMPARE(runtime.documentVersion().revision, before.revision + 3);
    historyManager->undo();
    QCOMPARE(created->clips().count(), 1);
    QVERIFY(editor.findClipItemById(clipId));

    controls = trackControls(editor, trackId);
    QVERIFY(controls);
    chooseTrackMenu(controls, controls->rect().center(), TrackControlView::tr("Delete"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(context->m_appModel->tracks().size(), 1);
    QCOMPARE(context->m_appModel->tracks().first()->id(), existingId);
    QVERIFY(!editor.findClipItemById(clipId));
    historyManager->undo();
    QCOMPARE(context->m_appModel->tracks().size(), 2);
    QCOMPARE(context->m_appModel->tracks().at(1)->id(), trackId);
    QVERIFY(trackControls(editor, trackId));
    QVERIFY(editor.findClipItemById(clipId));
    historyManager->undo();
    QVERIFY(!editor.findClipItemById(clipId));
    historyManager->undo();
    QCOMPARE(context->m_appModel->tracks().size(), 1);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::trackAudioMenuPreparesClipOrCancels_data() {
    QTest::addColumn<bool>("accept");
    QTest::newRow("open-and-decode") << true;
    QTest::newRow("cancel-file-picker") << false;
}

void ApplicationGuiTests::trackAudioMenuPreparesClipOrCancels() {
    QFETCH(bool, accept);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("短音.wav"));
    const auto error = createWaveFixture(path);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    TrackEditorView editor;
    const auto clearParent = qScopeGuard([] { trackController->setParentWidget(nullptr); });
    Automation::TrackDraftDto draft;
    draft.name = QStringLiteral("Audio target");
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, draft));
    auto *track = context->m_appModel->tracks().first();
    auto *canvas = editor.findChild<TracksGraphicsView *>();
    showTrackEditor(editor, canvas);
    if (QTest::currentTestFailed())
        return;
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto nativeDialogsDisabled = QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    const auto restoreDialogs = qScopeGuard(
        [&] { QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, nativeDialogsDisabled); });
    QTimer chooseFile;
    chooseFile.setInterval(10);
    QElapsedTimer waiting;
    bool choseFile = false;
    connect(&chooseFile, &QTimer::timeout, &editor, [&] {
        QPointer<QFileDialog> picker =
            qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
        if (!picker) {
            if (waiting.hasExpired(5000)) {
                chooseFile.stop();
                QFAIL("The audio file picker did not become active");
            }
            return;
        }
        chooseFile.stop();
        const auto closeOnFailure = qScopeGuard([&] {
            if (picker && QTest::currentTestFailed())
                picker->reject();
        });
        auto *name = picker->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"));
        QVERIFY(name);
        QTest::mouseClick(name, Qt::LeftButton);
        QTRY_VERIFY(name->hasFocus());
        QTest::keySequence(name, QKeySequence::SelectAll);
        QApplication::clipboard()->setText(QDir::toNativeSeparators(path));
        QTest::keySequence(name, QKeySequence::Paste);
        QCOMPARE(track->clips().count(), 0);
        QCOMPARE(runtime.documentVersion(), before);
        if (accept) {
            auto *buttons = picker->findChild<QDialogButtonBox *>();
            QVERIFY(buttons);
            auto *open = buttons->button(QDialogButtonBox::Open);
            QVERIFY(open && open->isEnabled());
            QTest::mouseClick(open, Qt::LeftButton);
        } else {
            QTest::keyClick(name, Qt::Key_Escape);
        }
        choseFile = true;
    });
    constexpr int start = 960;
    const auto position = canvas->mapFromScene(
        QPointF(canvas->sceneXForTick(start), TracksEditorGlobal::trackHeight * 0.5));
    waiting.start();
    chooseFile.start();
    chooseTrackMenu(canvas->viewport(), position,
                    TrackEditorContextMenuController::tr("Insert audio clip..."));
    chooseFile.stop();
    if (QTest::currentTestFailed())
        return;
    QVERIFY(choseFile);
    if (!accept) {
        QCOMPARE(track->clips().count(), 0);
        QCOMPARE(runtime.documentVersion(), before);
        QVERIFY(!historyManager->canUndo());
        QVERIFY(taskManager->tasks().isEmpty());
        return;
    }
    QTRY_COMPARE(track->clips().count(), 1);
    auto *audio = dynamic_cast<AudioClip *>(*track->clips().begin());
    QVERIFY(audio);
    QCOMPARE(QFileInfo(audio->path()).canonicalFilePath(), QFileInfo(path).canonicalFilePath());
    QCOMPARE(audio->audioInfo().frames, 800);
    QCOMPARE(audio->audioInfo().sampleRate, 8000);
    QCOMPARE(audio->audioInfo().channels, 1);
    QVERIFY(!audio->audioInfo().peakCache.isEmpty());
    QCOMPARE(audio->start(), start);
    QCOMPARE(audio->playLengthMs(), 100.0);
    QVERIFY(audio->hasRealTimeAnchor());
    QTRY_VERIFY(!audio->pathInfo().sha512.isEmpty());
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    const auto id = audio->id();
    QVERIFY(editor.findClipItemById(id));
    QVERIFY(runtime.documentVersion().revision > before.revision);
    historyManager->undo();
    QCOMPARE(track->clips().count(), 0);
    QVERIFY(!editor.findClipItemById(id));
    QVERIFY(!historyManager->canUndo());
    historyManager->redo();
    QTRY_COMPARE(track->clips().count(), 1);
    QVERIFY(editor.findClipItemById(id));
}
