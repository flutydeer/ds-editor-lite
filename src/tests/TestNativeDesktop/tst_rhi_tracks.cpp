#include "tst_native_desktop.h"
#include "NativeAppFixture.h"

#include "Automation/CoreRuntime.h"
#include "Controller/ClipController.h"
#include "Controller/TrackController.h"
#include "Global/TracksEditorGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/TrackEditor/TracksRhiWidget.h"

#include <lite/History/ActionSequence.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QtTest/QTest>

namespace {
    struct TrackFixture {
        NativeAppFixture application;
        std::unique_ptr<TracksRhiWidget> canvas;
        int clipId = -1;
        int firstTrackId = -1;
        int secondTrackId = -1;

        ~TrackFixture() {
            canvas.reset();
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

        bool initialize() {
            if (!application.initialize())
                return false;
            auto document = Automation::DocumentAutomationFacade::newDocumentDraft(false);
            Automation::ClipDraftDto clip;
            clip.properties.name = QStringLiteral("Movable phrase");
            clip.properties.start = 480;
            clip.properties.length = 1920;
            clip.properties.clipLen = 960;
            clip.defaultLanguage = QStringLiteral("eng");
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
            canvas = std::make_unique<TracksRhiWidget>();
            canvas->setApi(QRhiWidget::Api::Null);
            canvas->resize(1000, 400);
            canvas->show();
            canvas->activateWindow();
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
