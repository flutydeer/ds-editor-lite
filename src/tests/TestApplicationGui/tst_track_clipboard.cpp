#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/TrackController.h"
#include "Global/ControllerGlobal.h"
#include "Global/TracksEditorGlobal.h"
#include "UI/Views/TrackEditor/GraphicsItem/AbstractClipView.h"
#include "UI/Views/TrackEditor/TrackEditorContextMenuController.h"
#include "UI/Views/TrackEditor/TrackEditorView.h"
#include "UI/Views/TrackEditor/TracksGraphicsView.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMimeData>
#include <QScopeGuard>
#include <QTimer>
#include <QtTest/QTest>

void ApplicationGuiTests::trackContextMenuPastePreviewCancelsAndMatchesCommittedClip() {
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
    clip.notes.append(note);
    Automation::TrackDraftDto sourceDraft;
    sourceDraft.name = QStringLiteral("Source");
    sourceDraft.clips.append(clip);
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, sourceDraft));
    Automation::TrackDraftDto destinationDraft;
    destinationDraft.name = QStringLiteral("Destination");
    QVERIFY(runtime.project().insertTrack(commandContext(), 1, destinationDraft));
    auto *sourceTrack = context->m_appModel->tracks().at(0);
    auto *destination = context->m_appModel->tracks().at(1);
    const auto *source = dynamic_cast<SingingClip *>(*sourceTrack->clips().begin());
    QVERIFY(source);
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
        QContextMenuEvent event(QContextMenuEvent::Mouse, position,
                                canvas->viewport()->mapToGlobal(position));
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
            QTest::mouseMove(&menu, position);
            QTRY_COMPARE(previewItems().size(), 1);
            const auto *preview = previewItems().first();
            QCOMPARE(preview->trackIndex(), 1);
            QCOMPARE(preview->length(), source->length());
            previewStart = preview->start();
            previewRect = preview->sceneBoundingRect();
            QCOMPARE(destination->clips().count(), 0);
            QCOMPARE(sourceTrack->clips().count(), 1);
            QCOMPARE(runtime.documentVersion(), before);
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
    const auto *pasted = dynamic_cast<SingingClip *>(*destination->clips().begin());
    QVERIFY(pasted);
    QCOMPARE(pasted->start(), previewStart);
    QCOMPARE(pasted->length(), source->length());
    QCOMPARE(pasted->notes().count(), 1);
    QCOMPARE((*pasted->notes().begin())->localStart(), note.localStart);
    QCOMPARE((*pasted->notes().begin())->lyric(), note.lyric);
    const auto pastedId = pasted->id();
    const auto *pastedItem = editor.findClipItemById(pastedId);
    QVERIFY(pastedItem);
    QCOMPARE(pastedItem->sceneBoundingRect(), previewRect);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(destination->clips().count(), 0);
    QCOMPARE(sourceTrack->clips().count(), 1);
    QCOMPARE(source->start(), clip.properties.start);
    QVERIFY(!editor.findClipItemById(pastedId));
    QVERIFY(previewItems().isEmpty());
    QVERIFY(!historyManager->canUndo());
    QCOMPARE(runtime.documentVersion().revision, before.revision + 2);
}
