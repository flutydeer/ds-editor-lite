#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/TrackController.h"
#include "Global/ControllerGlobal.h"
#include "Global/TracksEditorGlobal.h"
#include "UI/Views/TrackEditor/GraphicsItem/AbstractClipView.h"
#include "UI/Views/TrackEditor/GraphicsItem/AudioClipView.h"
#include "UI/Views/TrackEditor/TrackEditorContextMenuController.h"
#include "UI/Views/TrackEditor/TrackEditorView.h"
#include "UI/Views/TrackEditor/TracksGraphicsView.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/Tasking/TaskManager.h>
#include "../TestSupport/WaveFixture.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QCursor>
#include <QMenu>
#include <QMimeData>
#include <QScopeGuard>
#include <QTimer>
#include <QTemporaryDir>
#include <QWindow>
#include <QtTest/QTest>

void ApplicationGuiTests::trackContextMenuPastePreviewCancelsAndMatchesCommittedClip_data() {
    QTest::addColumn<bool>("audio");
    QTest::newRow("singing-clip") << false;
    QTest::newRow("decoded-audio-clip") << true;
}

void ApplicationGuiTests::trackContextMenuPastePreviewCancelsAndMatchesCommittedClip() {
    QFETCH(bool, audio);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    auto previousClipboard = std::make_unique<QMimeData>();
    if (const auto *mime = QApplication::clipboard()->mimeData()) {
        for (const auto &format : mime->formats())
            previousClipboard->setData(format, mime->data(format));
    }
    const auto restoreClipboard =
        qScopeGuard([&] { QApplication::clipboard()->setMimeData(previousClipboard.release()); });
    const auto previousCursor = QCursor::pos();
    const auto restoreCursor = qScopeGuard([&] { QCursor::setPos(previousCursor); });
    const auto releaseAudio = qScopeGuard([&] {
        if (!audio)
            return;
        QVERIFY(runtime.documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        if (QTest::currentTestFailed())
            files.setAutoRemove(false);
    });
    TrackEditorView editor;
    const auto clearParent = qScopeGuard([] { trackController->setParentWidget(nullptr); });
    auto *canvas = editor.findChild<TracksGraphicsView *>();
    QVERIFY(canvas);
    const auto clearInteraction = qScopeGuard([&] {
        canvas->discardAction();
        canvas->clearTrackPastePreview();
    });
    Automation::NoteDraftDto note;
    note.localStart = 120;
    note.length = 480;
    note.keyIndex = 60;
    note.lyric = QStringLiteral("la");
    note.language = QStringLiteral("eng");
    Automation::ClipDraftDto clip;
    clip.type = Automation::ClipDraftDto::Type::Singing;
    clip.properties.name = QStringLiteral("Copied clip");
    clip.properties.start = 480;
    clip.properties.length = 960;
    clip.properties.clipLen = 960;
    clip.defaultLanguage = note.language;
    clip.properties.gain = 0.625;
    if (audio) {
        clip.type = Automation::ClipDraftDto::Type::Audio;
        clip.audioPath = files.filePath(QStringLiteral("paste.wav"));
        QVERIFY(TestSupport::writeWave(clip.audioPath, QVector<float>(48000, 0.125f)));
        clip.hasRealTimeAnchor = true;
        clip.properties.trimStartMs = 125;
        clip.properties.playLengthMs = 750;
        clip.properties.materialLengthMs = 1000;
    } else {
        clip.notes.append(note);
    }
    Automation::TrackDraftDto sourceDraft;
    sourceDraft.name = QStringLiteral("Source");
    sourceDraft.clips.append(clip);
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, sourceDraft));
    Automation::TrackDraftDto destinationDraft;
    destinationDraft.name = QStringLiteral("Destination");
    QVERIFY(runtime.project().insertTrack(commandContext(), 1, destinationDraft));
    auto *sourceTrack = context->m_appModel->tracks().at(0);
    auto *destination = context->m_appModel->tracks().at(1);
    const auto *source = *sourceTrack->clips().begin();
    QVERIFY(source);
    const auto *sourceAudio = qobject_cast<const AudioClip *>(source);
    if (audio) {
        QVERIFY(sourceAudio);
        QTRY_COMPARE(sourceAudio->audioInfo().frames, 48000);
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    } else {
        QVERIFY(qobject_cast<const SingingClip *>(source));
    }
    const auto originalSourceStart = source->start();
    auto *sourceItem = editor.findClipItemById(source->id());
    QVERIFY(sourceItem);
    editor.resize(1200, 500);
    canvas->setAnimationEnabled(false);
    editor.show();
    editor.activateWindow();
    canvas->setFocus();
    QTRY_VERIFY(editor.isVisible() && canvas->viewport()->width() > 600);
    QVERIFY(canvas->setViewportScale(2, 1));
    canvas->setViewportStartTick(0);
    QCoreApplication::processEvents();
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto beforeContents = TestSupport::projectSnapshot(*context->m_appModel);
    const auto sourcePosition = canvas->mapFromScene(sourceItem->sceneBoundingRect().center());
    QVERIFY(canvas->viewport()->rect().contains(sourcePosition));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, sourcePosition);
    QCOMPARE(canvas->selectedClipsId(), QList<int>{source->id()});
    const auto runMenu = [&](const QPoint &position, const auto &interact) {
        bool entered = false;
        QTimer action;
        action.setSingleShot(true);
        connect(&action, &QTimer::timeout, &editor, [&] {
            const auto closeMenu = qScopeGuard([&] {
                for (auto *owned :
                     editor.findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly)) {
                    if (owned->isVisible())
                        owned->close();
                }
            });
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(menu);
            QCOMPARE(menu->parentWidget(), &editor);
            entered = true;
            interact(*menu);
        });
        const auto globalPosition = canvas->viewport()->mapToGlobal(position);
        QCursor::setPos(globalPosition);
        // Cursor warping can be a no-op after a synthetic popup move; also update Qt's input
        // position.
        QTest::mouseMove(editor.windowHandle(), editor.mapFromGlobal(globalPosition));
        QContextMenuEvent event(QContextMenuEvent::Mouse, position, globalPosition);
        action.start(0);
        QApplication::sendEvent(canvas->viewport(), &event);
        action.stop();
        QVERIFY(entered);
    };
    const auto menuAction = [](QMenu &menu, const QString &text) -> QAction * {
        for (auto *action : menu.actions()) {
            if (action->text() == text)
                return action;
        }
        return nullptr;
    };
    runMenu(sourcePosition, [&](QMenu &menu) {
        auto *copy = menuAction(menu, TrackEditorContextMenuController::tr("&Copy"));
        QVERIFY(copy);
        QVERIFY(copy->isEnabled());
        QTest::mouseClick(&menu, Qt::LeftButton, Qt::NoModifier,
                          menu.actionGeometry(copy).center());
    });
    if (QTest::currentTestFailed())
        return;
    QVERIFY(QApplication::clipboard()->mimeData()->hasFormat(
        ControllerGlobal::ElemMimeType.at(ControllerGlobal::Clip)));
    QCOMPARE(runtime.documentVersion(), before);
    const auto targetPosition = canvas->mapFromScene(QPointF(
        canvas->sceneXForTick(2180), sourceItem->sceneBoundingRect().center().y() +
                                         TracksEditorGlobal::trackHeight * canvas->scaleY()));
    QVERIFY(canvas->viewport()->rect().contains(targetPosition));
    const auto previewItems = [&] {
        QList<AbstractClipView *> items;
        for (auto *item : canvas->scene()->items()) {
            if (auto *preview = dynamic_cast<AbstractClipView *>(item);
                preview && preview->id() < 0)
                items.append(preview);
        }
        return items;
    };
    int previewStart = -1;
    QRectF previewRect;
    const auto previewAndFinish = [&](bool commit) {
        runMenu(targetPosition, [&](QMenu &menu) {
            auto *paste = menuAction(menu, TrackEditorContextMenuController::tr("&Paste"));
            QVERIFY(paste);
            QVERIFY(paste->isEnabled());
            const auto position = menu.actionGeometry(paste).center();
            QVERIFY(menu.windowHandle());
            // Target the popup directly; offscreen cursor warping can select its owner instead.
            QTest::mouseMove(menu.windowHandle(), position);
            QTRY_COMPARE(menu.activeAction(), paste);
            QTRY_COMPARE(previewItems().size(), 1);
            const auto *preview = previewItems().first();
            QCOMPARE(preview->trackIndex(), 1);
            QCOMPARE(preview->length(), source->length());
            if (audio) {
                const auto *audioPreview = dynamic_cast<const AudioClipView *>(preview);
                QVERIFY(audioPreview);
                QCOMPARE(audioPreview->path(), sourceAudio->path());
                QVERIFY(audioPreview->contentLength() > 0);
            }
            QVERIFY(!preview->sceneBoundingRect().isEmpty());
            previewStart = preview->start();
            previewRect = preview->sceneBoundingRect();
            QCOMPARE(destination->clips().count(), 0);
            QCOMPARE(sourceTrack->clips().count(), 1);
            QCOMPARE(runtime.documentVersion(), before);
            QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeContents);
            QVERIFY(!historyManager->canUndo());
            if (commit)
                QTest::mouseClick(&menu, Qt::LeftButton, Qt::NoModifier, position);
            else
                QTest::keyClick(&menu, Qt::Key_Escape);
        });
    };
    previewAndFinish(false);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(previewItems().isEmpty());
    QCOMPARE(destination->clips().count(), 0);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    previewAndFinish(true);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(previewItems().isEmpty());
    QCOMPARE(destination->clips().count(), 1);
    const auto *pasted = *destination->clips().begin();
    QVERIFY(pasted);
    QCOMPARE(pasted->start(), previewStart);
    QCOMPARE(pasted->length(), source->length());
    QCOMPARE(pasted->gain(), source->gain());
    if (audio) {
        const auto *pastedAudio = qobject_cast<const AudioClip *>(pasted);
        QVERIFY(pastedAudio);
        QCOMPARE(pastedAudio->path(), sourceAudio->path());
        QCOMPARE(pastedAudio->trimStartMs(), sourceAudio->trimStartMs());
        QCOMPARE(pastedAudio->playLengthMs(), sourceAudio->playLengthMs());
        QCOMPARE(pastedAudio->materialLengthMs(), sourceAudio->materialLengthMs());
    } else {
        const auto *pastedSinging = qobject_cast<const SingingClip *>(pasted);
        QVERIFY(pastedSinging);
        QCOMPARE(pastedSinging->notes().count(), 1);
        QCOMPARE((*pastedSinging->notes().begin())->localStart(), note.localStart);
        QCOMPARE((*pastedSinging->notes().begin())->lyric(), note.lyric);
    }
    const auto pastedId = pasted->id();
    const auto *pastedItem = editor.findClipItemById(pastedId);
    QVERIFY(pastedItem);
    QCOMPARE(pastedItem->sceneBoundingRect(), previewRect);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(destination->clips().count(), 0);
    QCOMPARE(sourceTrack->clips().count(), 1);
    QCOMPARE(source->start(), originalSourceStart);
    QVERIFY(!editor.findClipItemById(pastedId));
    QVERIFY(previewItems().isEmpty());
    QVERIFY(!historyManager->canUndo());
    QCOMPARE(runtime.documentVersion().revision, before.revision + 2);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeContents);
    QVERIFY(runtime.history().redo(commandContext()));
    const auto *redone = editor.findClipItemById(pastedId);
    QVERIFY(redone);
    QCOMPARE(redone->sceneBoundingRect(), previewRect);
    QCOMPARE(destination->clips().count(), 1);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 3);
}
