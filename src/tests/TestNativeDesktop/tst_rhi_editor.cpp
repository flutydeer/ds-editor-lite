#include "tst_native_desktop.h"
#include "../TestSupport/GuiAppFixture.h"
#include "../TestSupport/VoicebankFixture.h"
#include "../TestSupport/PointerEvents.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Bootstrap/AppEnvironment.h"
#include "Controller/ClipController.h"
#include "Controller/TrackController.h"
#include "Global/TracksEditorGlobal.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/OutputSystem.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollRhiWidget.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoKeyboardView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsViewHelper.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollCoord.h"
#include "UI/Views/ClipEditor/ClipEditorView.h"
#include "UI/Views/ClipEditor/ParamEditor/ParamEditorGraphicsView.h"
#include "UI/Views/ClipEditor/ParamEditor/ParamEditorView.h"
#include "UI/Views/TrackEditor/TracksRhiWidget.h"
#include "UI/Views/TrackEditor/InfoLane/InfoLaneView.h"
#include "UI/Views/Common/TimelineView.h"
#include "UI/Window/MainWindow.h"
#include "UI/Views/BottomPanelView.h"
#include "UI/Views/Common/TabPanelTitleBar.h"
#include "UI/Dialogs/Base/Dialog.h"

#include <lite/GUI/Controls/Toast.h>
#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ToolTip.h>
#include <lite/GUI/Controls/OverlayScrollBar.h>

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
#include <QDir>
#include <QFile>
#include <QContextMenuEvent>
#include <QCursor>
#include <QEventLoop>
#include <QFileInfo>
#include <QLineEdit>
#include <QMenu>
#include <QMap>
#include <QMouseEvent>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTimer>
#include <QTouchEvent>
#include <QWindow>
#include <QWheelEvent>
#include <QNativeGestureEvent>
#include <QPointingDevice>
#include <QtTest/QTest>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>
#include <QtGui/private/qhighdpiscaling_p.h>
#include <qpa/qwindowsysteminterface.h>
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
#include <QOffscreenSurface>
#endif

#include <algorithm>
#include <cmath>

namespace {
    QString windowInputState(const QWidget &owner, const QWidget &target) {
        const auto describe = [](const QWidget *widget) {
            return widget
                       ? QStringLiteral("%1(%2)").arg(
                             QLatin1String(widget->metaObject()->className()), widget->objectName())
                       : QStringLiteral("none");
        };
        const auto *handle = owner.windowHandle();
        return QStringLiteral("ownerActive=%1 exposed=%2 appState=%3 targetVisible=%4 "
                              "targetFocus=%5 modal=%6 popup=%7 focus=%8 active=%9")
            .arg(owner.isActiveWindow())
            .arg(handle && handle->isExposed())
            .arg(static_cast<int>(QGuiApplication::applicationState()))
            .arg(target.isVisibleTo(&owner))
            .arg(target.hasFocus())
            .arg(describe(QApplication::activeModalWidget()),
                 describe(QApplication::activePopupWidget()), describe(QApplication::focusWidget()),
                 describe(QApplication::activeWindow()));
    }

    QRhiWidget::Api platformRhiApi() {
#if defined(Q_OS_WIN)
        return QRhiWidget::Api::Direct3D11;
#elif defined(Q_OS_MACOS)
        return QRhiWidget::Api::Metal;
#else
        return QRhiWidget::Api::OpenGL;
#endif
    }

    bool platformRhiAvailable() {
#if defined(Q_OS_WIN)
        QRhiD3D11InitParams params;
        std::unique_ptr<QRhi> device(QRhi::create(
            QRhi::D3D11, &params, QRhi::PreferSoftwareRenderer | QRhi::SuppressSmokeTestWarnings));
#elif defined(Q_OS_MACOS)
        QRhiMetalInitParams params;
        std::unique_ptr<QRhi> device(
            QRhi::create(QRhi::Metal, &params, QRhi::SuppressSmokeTestWarnings));
#else
        std::unique_ptr<QOffscreenSurface> surface(QRhiGles2InitParams::newFallbackSurface());
        QRhiGles2InitParams params;
        params.fallbackSurface = surface.get();
        std::unique_ptr<QRhi> device(
            QRhi::create(QRhi::OpenGLES2, &params, QRhi::SuppressSmokeTestWarnings));
#endif
        return bool(device);
    }

    auto preferSoftwareRhi(bool enabled) {
        const auto previous = qgetenv("QSG_RHI_PREFER_SOFTWARE_RENDERER");
#ifdef Q_OS_WIN
        if (enabled)
            qputenv("QSG_RHI_PREFER_SOFTWARE_RENDERER", "1");
#else
        Q_UNUSED(enabled)
#endif
        return qScopeGuard([previous] {
            if (previous.isNull())
                qunsetenv("QSG_RHI_PREFER_SOFTWARE_RENDERER");
            else
                qputenv("QSG_RHI_PREFER_SOFTWARE_RENDERER", previous);
        });
    }

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

        void configureInference() {
            const auto root = TestSupport::voicebankRoot();
            QVERIFY2(QFileInfo(root).isAbsolute() && QFileInfo(root).isDir(), qPrintable(root));
            QVERIFY(!TestSupport::fixtureLanguage().isEmpty());
            QVERIFY(!TestSupport::fixtureLyric().isEmpty());
            packageManager->initialize({root});
            QTRY_COMPARE_WITH_TIMEOUT(appStatus->packageModuleStatus.get(),
                                      AppStatus::ModuleStatus::Ready, 10000);
            SingerInfo singer;
            for (const auto &package : packageManager->installedPackages().successfulPackages)
                for (const auto &candidate : package.singers())
                    if (candidate.singerId() == TestSupport::fixtureSingerId())
                        singer = candidate;
            QVERIFY(!singer.isEmpty() && !singer.speakers().isEmpty());
            const auto clipId = Automation::ClipId(clip->id());
            QVERIFY(runtime().parameters().selectClipSingleSpeaker(command(), clipId, singer,
                                                                   singer.speakers().first()));
            QVERIFY(runtime().notes().patchWordProperties(
                command(), clipId,
                {
                    {.noteId = Automation::NoteId(noteId),
                     .lyric = TestSupport::fixtureLyric(),
                     .language = TestSupport::fixtureLanguage()}
            }));
            QTRY_VERIFY_WITH_TIMEOUT(inferenceSettled(), 15000);
        }

        bool inferenceSettled() const {
            qsizetype includedNotes = 0;
            for (const auto *piece : clip->pieces())
                includedNotes += piece->notes.size();
            return !clip->pieces().isEmpty() &&
                   includedNotes + clip->noteInferenceErrors().size() == clip->notes().count() &&
                   taskManager->tasks().isEmpty() &&
                   std::all_of(clip->pieces().cbegin(), clip->pieces().cend(),
                               [](const InferPiece *piece) {
                                   return piece->state == QStringLiteral("Acoustic.Awaiting") ||
                                          piece->state == QStringLiteral("Ready");
                               });
        }

        void initialize(const int clipLength = 3840, QRhiWidget::Api api = QRhiWidget::Api::Null) {
            QVERIFY2(app.initialize(), qPrintable(app.error));
            Automation::NoteDraftDto note;
            note.localStart = 480;
            note.length = 480;
            note.keyIndex = 60;
            note.lyric = QStringLiteral("la");
            note.language = QStringLiteral("eng");
            Automation::ClipDraftDto draft;
            draft.type = Automation::ClipDraftDto::Type::Singing;
            draft.properties.length = clipLength;
            draft.properties.clipLen = clipLength;
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
            canvas->setApi(api);
            canvas->setDataContext(clip);
            canvas->setEditMode(ClipEditorGlobal::Select);
            QObject::connect(canvas.get(), &EditorRhiWidget::backendFailed, canvas.get(),
                             [this](const QString &reason) { backendError = reason; });
            submitted = std::make_unique<QSignalSpy>(canvas.get(), &QRhiWidget::frameSubmitted);
            TestSupport::placeWindowOnScreen(*canvas, {900, 500});
            canvas->show();
            QTRY_VERIFY2(canvas->windowHandle() && canvas->windowHandle()->isExposed(),
                         qPrintable(windowInputState(*canvas, *canvas)));
            // Cocoa activation alone does not bring an inactive application forward.
            canvas->raise();
            canvas->activateWindow();
            QTRY_VERIFY2(canvas->isActiveWindow(), qPrintable(windowInputState(*canvas, *canvas)));
            waitForFrame();
            if (QTest::currentTestFailed())
                return;
            canvas->setFocus();
            QTRY_VERIFY2(canvas->hasFocus(), qPrintable(windowInputState(*canvas, *canvas)));
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
            QVERIFY(
                runtime().notes().insertNotes(command(), Automation::ClipId(clip->id()), {draft}));
            QCOMPARE(clip->notes().count(), 2);
            for (const auto *note : clip->notes()) {
                if (note->id() != noteId)
                    secondNoteId = note->id();
            }
            QVERIFY(secondNoteId >= 0);
            waitForFrame();
            historyManager->reset();
        }

        void moveTo(const QPoint &position) const {
            // Edge scrolling reads the native cursor while waiting for a frame.
            QCursor::setPos(canvas->mapToGlobal(position));
            // Deliver pending platform movement before the synthetic drag position.
            QCoreApplication::processEvents();
            QTest::mouseMove(canvas.get(), position);
        }

        void hoverAt(const QPoint &position) const {
            moveTo(position);
            // QWidget's hover overload only moves the cursor and may not deliver an event.
            QTest::mouseMove(canvas->windowHandle(), position);
        }

        void waitForFrame(QEventLoop::ProcessEventsFlags flags = QEventLoop::AllEvents) const {
            // Input dispatch may already have presented an earlier pending frame.
            const auto count = submitted->size();
            QEventLoop loop;
            QTimer timeout;
            timeout.setSingleShot(true);
            QObject::connect(canvas.get(), &QRhiWidget::frameSubmitted, &loop, &QEventLoop::quit);
            QObject::connect(canvas.get(), &EditorRhiWidget::backendFailed, &loop,
                             &QEventLoop::quit);
            QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
            canvas->update();
            if (submitted->size() == count && backendError.isEmpty()) {
                timeout.start(5000);
                loop.exec(flags);
            }
            QVERIFY2(backendError.isEmpty(), qPrintable(backendError));
            QVERIFY(submitted->size() > count);
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

void NativeDesktopTests::rhiGhostReferencesUpdateFramesWithoutOwningEdits_data() {
    QTest::addColumn<bool>("scaleDisplay");
    QTest::newRow("reference-editing") << false;
    QTest::newRow("scaled-native-renderer") << true;
}

void NativeDesktopTests::rhiGhostReferencesUpdateFramesWithoutOwningEdits() {
    QFETCH(bool, scaleDisplay);
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    if (scaleDisplay && !platformRhiAvailable())
        QSKIP("No graphics backend is available for framebuffer validation");
    const auto restoreSoftware = preferSoftwareRhi(scaleDisplay);
    ExistingRhiNoteFixture fixture;
    fixture.initialize(3840, scaleDisplay ? platformRhiApi() : QRhiWidget::Api::Null);
    if (QTest::currentTestFailed())
        return;
    auto *screen = fixture.canvas->screen();
    const auto originalFactor =
        QHighDpiScaling::factor(screen) / QHighDpiScaling::factor(static_cast<QScreen *>(nullptr));
    const auto originalDpr = fixture.canvas->devicePixelRatioF();
    const auto originalFrameSize =
        scaleDisplay ? fixture.canvas->grabFramebuffer().size() : QSize{};
    const auto setDisplayFactor = [&](qreal factor) {
        QHighDpiScaling::setScreenFactor(screen, factor);
        QWindowSystemInterface::handleWindowDevicePixelRatioChanged<
            QWindowSystemInterface::SynchronousDelivery>(fixture.canvas->windowHandle());
    };
    const auto restoreScale = qScopeGuard([&] {
        if (scaleDisplay) {
            setDisplayFactor(originalFactor);
            QCoreApplication::processEvents();
        }
    });
    auto &runtime = fixture.runtime();
    const auto settings = runtime.settings().getSettings();
    QVERIFY(settings);
    const auto originalAppearance = settings.get().appearance;
    const auto restore =
        qScopeGuard([&] { QVERIFY(runtime.settings().updateAppearance({}, originalAppearance)); });
    auto appearance = originalAppearance;
    appearance.showGhostNotes = true;
    QVERIFY(runtime.settings().updateAppearance({}, appearance));
    Automation::NoteDraftDto note;
    note.localStart = 1920;
    note.length = 240;
    note.keyIndex = 60;
    note.lyric = QStringLiteral("la");
    note.language = QStringLiteral("eng");
    Automation::ClipDraftDto draft;
    draft.properties.length = 3840;
    draft.properties.clipLen = 3840;
    draft.defaultLanguage = note.language;
    draft.notes = {note};
    Automation::TrackDraftDto track;
    track.name = QStringLiteral("RHI reference");
    track.clips = {draft};
    const auto beforeInsertFrame = fixture.submitted->size();
    const auto inserted = runtime.project().insertTrack(fixture.command(), 1, track);
    QVERIFY(inserted);
    auto *reference = qobject_cast<SingingClip *>(
        *fixture.app.context->m_appModel->tracks().at(1)->clips().begin());
    QVERIFY(reference);
    QCOMPARE(reference->notes().count(), 1);
    const auto referenceId = (*reference->notes().begin())->id();
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    QTRY_VERIFY(fixture.submitted->size() > beforeInsertFrame);
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto model = TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
    for (const bool enabled : {false, true}) {
        const auto frames = fixture.submitted->size();
        appearance.showGhostNotes = enabled;
        QVERIFY(runtime.settings().updateAppearance({}, appearance));
        QTRY_VERIFY(fixture.submitted->size() > frames);
    }
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), model);
    QVERIFY(!historyManager->canUndo());
    if (scaleDisplay) {
        QVERIFY(!originalFrameSize.isEmpty());
        setDisplayFactor(originalFactor * 1.25);
        fixture.waitForFrame();
        if (QTest::currentTestFailed())
            return;
        QTRY_VERIFY(qFuzzyCompare(fixture.canvas->devicePixelRatioF(), originalDpr * 1.25));
        const auto scaledFrame = fixture.canvas->grabFramebuffer();
        QVERIFY(!scaledFrame.isNull());
        QVERIFY(scaledFrame.size() != originalFrameSize);
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), model);
        QVERIFY(!historyManager->canUndo());
    }
    fixture.canvas->setEditMode(ClipEditorGlobal::DrawNote);
    const auto position = fixture.pointFor(2040, 60);
    QVERIFY(fixture.canvas->rect().contains(position));
    fixture.moveTo(position);
    QTest::mousePress(fixture.canvas.get(), Qt::LeftButton, Qt::NoModifier, position);
    QTest::mouseRelease(fixture.canvas.get(), Qt::LeftButton, Qt::NoModifier, position);
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(fixture.clip->notes().count(), 2);
    QCOMPARE(reference->notes().count(), 1);
    QCOMPARE((*reference->notes().begin())->id(), referenceId);
    QCOMPARE((*reference->notes().begin())->localStart(), 1920);
    const auto drawn =
        std::find_if(fixture.clip->notes().begin(), fixture.clip->notes().end(),
                     [&](const Note *value) { return value->id() != fixture.noteId; });
    QVERIFY(drawn != fixture.clip->notes().end());
    QCOMPARE((*drawn)->keyIndex(), 60);
    QVERIFY((*drawn)->localStart() >= 1920 && (*drawn)->localStart() < 2160);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QVERIFY(runtime.history().undo(fixture.command()));
    fixture.waitForFrame();
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), model);
    QVERIFY(!historyManager->canUndo());
    if (scaleDisplay) {
        setDisplayFactor(originalFactor);
        fixture.waitForFrame();
        if (QTest::currentTestFailed())
            return;
        QTRY_VERIFY(qFuzzyCompare(fixture.canvas->devicePixelRatioF(), originalDpr));
        QCOMPARE(fixture.canvas->grabFramebuffer().size(), originalFrameSize);
        QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), model);
    }
}

void NativeDesktopTests::rhiThemeAndDockingPreserveBothEditorsAndTheirDocument() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    GuiDocumentFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.error));
    auto *themes = ThemeManager::instance();
    const auto previousTheme = themes->currentThemeId();
    const auto backend = appOptions->developer()->editorRenderBackend;
    const auto nativeFrame = appOptions->appearance()->useNativeFrame;
    const auto detachEnabled = appOptions->developer()->enablePanelDetach;
    const auto restore = qScopeGuard([&] {
        appOptions->developer()->editorRenderBackend = backend;
        appOptions->appearance()->useNativeFrame = nativeFrame;
        appOptions->developer()->enablePanelDetach = detachEnabled;
        themes->applyTheme(previousTheme);
    });
    appOptions->developer()->editorRenderBackend =
        DeveloperOption::EditorRenderBackend::RhiExperimental;
    appOptions->appearance()->useNativeFrame = true;
    appOptions->developer()->enablePanelDetach = true;
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
    window.raise();
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
    const auto model = TestSupport::projectSnapshot(*fixture.context->m_appModel);
    {
        QVERIFY(editor->setRegionVisibility(true, true));
        QVERIFY(window.setParameterForeground(ParamInfo::MouthOpening));
        QVERIFY(window.setPianoRollScale(2, 1));
        QVERIFY(window.centerPianoRollAt(1920, 60));
        auto *panel = window.findChild<ParamEditorView *>();
        QVERIFY(panel && panel->isVisible());
        auto *parameters = panel->graphicsView();
        QVERIFY(parameters && parameters->isVisible());
        auto *viewport = parameters->viewport();
        const QPointF originalScale(piano->scaleX(), piano->scaleY());
        const auto originalValues = panel->viewState();
        const auto verifyTimeline = [&] {
            QCOMPARE(QPointF(piano->scaleX(), piano->scaleY()), originalScale);
            QTRY_COMPARE(parameters->scaleX(), piano->scaleX());
            const auto ticksPerPixel = (piano->endTick() - piano->startTick()) / piano->width();
            QTRY_VERIFY(std::abs(parameters->startTick() - piano->startTick()) <= ticksPerPixel);
            QCOMPARE(panel->viewState().centerRatio, originalValues.centerRatio);
            QCOMPARE(panel->viewState().verticalScale, originalValues.verticalScale);
            QCOMPARE(runtime.documentVersion(), before);
            QCOMPARE(TestSupport::projectSnapshot(*fixture.context->m_appModel), model);
            QVERIFY(!historyManager->canUndo());
            QVERIFY(!editSessionManager->hasActiveTransaction());
        };
        const auto position = viewport->rect().center();
        const auto beforeWheel = piano->startTick();
        QWheelEvent wheel(position, viewport->mapToGlobal(position), QPoint(0, -80), {},
                          Qt::NoButton, Qt::ShiftModifier, Qt::ScrollUpdate, false);
        QApplication::sendEvent(viewport, &wheel);
        QTRY_VERIFY(piano->startTick() > beforeWheel);
        verifyTimeline();
        if (QTest::currentTestFailed())
            return;
        const auto fingerEditing = appOptions->general()->drawParamWithFinger;
        auto *touchDevice = QTest::createTouchDevice();
        const auto restoreInput = qScopeGuard([&] {
            QTouchEvent cancel(QEvent::TouchCancel, touchDevice);
            QApplication::sendEvent(viewport, &cancel);
            appOptions->general()->drawParamWithFinger = fingerEditing;
        });
        appOptions->general()->drawParamWithFinger = false;
        const auto beforePan = piano->startTick();
        const auto beforeFrame = pianoFrames.size();
        auto *inputWindow = viewport->window()->windowHandle();
        QVERIFY(inputWindow);
        const auto inputPosition = [&](const QPoint &point) {
            return inputWindow->mapFromGlobal(viewport->mapToGlobal(point));
        };
        auto touch = QTest::touchEvent(inputWindow, touchDevice, false);
        touch.press(0, inputPosition(position)).commit();
        touch.move(0, inputPosition(position - QPoint(24, 0))).commit();
        touch.move(0, inputPosition(position - QPoint(48, 0))).commit();
        QTRY_VERIFY2(piano->startTick() > beforePan, qPrintable(recentInput.join('\n')));
        touch.release(0, inputPosition(position - QPoint(48, 0))).commit();
        verifyTimeline();
        if (QTest::currentTestFailed())
            return;
        QTRY_VERIFY(pianoFrames.size() > beforeFrame);
        QVERIFY(window.setPianoRollScale(1, 1));
        QVERIFY(window.centerPianoRollAt(1920, 60));
    }
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
    {
        QTemporaryDir externalThemes;
        QVERIFY(externalThemes.isValid());
        const auto themeId = themes->currentThemeId();
        const auto themeDirectory = externalThemes.filePath(themeId);
        QVERIFY(QDir().mkpath(themeDirectory));
        QFile manifest(QDir(themeDirectory).filePath(QStringLiteral("manifest.json")));
        QVERIFY(manifest.open(QIODevice::WriteOnly));
        QCOMPARE(manifest.write("{}"), qint64{2});
        manifest.close();
        const auto previousRoot = qgetenv("DS_EDITOR_THEME_DIR");
        const auto restoreRoot = [&] {
            if (previousRoot.isNull())
                qunsetenv("DS_EDITOR_THEME_DIR");
            else
                qputenv("DS_EDITOR_THEME_DIR", previousRoot);
        };
        const auto restoreThemeRoot = qScopeGuard(restoreRoot);
        QVERIFY(ThemeLoader::load(themeId));
        QVERIFY(ThemeLoader::lastError().isEmpty());
        const auto originalStyle = window.styleSheet();
        const auto originalWhiteKey = piano->property("whiteKeyColor").value<QColor>();
        const auto originalBackground = tracks->property("backgroundColor").value<QColor>();
        QSignalSpy changed(themes, &ThemeManager::themeChanged);
        qputenv("DS_EDITOR_THEME_DIR", externalThemes.path().toUtf8());
        QTest::keyClick(&window, Qt::Key_F5, Qt::ControlModifier | Qt::ShiftModifier);
        QTRY_VERIFY(ThemeLoader::lastError().contains(themeDirectory));
        QCOMPARE(changed.size(), 0);
        QCOMPARE(themes->currentThemeId(), themeId);
        QCOMPARE(window.styleSheet(), originalStyle);
        QCOMPARE(piano->property("whiteKeyColor").value<QColor>(), originalWhiteKey);
        QCOMPARE(tracks->property("backgroundColor").value<QColor>(), originalBackground);
        verifyTimelines();
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*fixture.context->m_appModel), model);
        QVERIFY(!historyManager->canUndo());
        restoreRoot();
        const auto pianoBeforeReload = pianoFrames.size();
        const auto tracksBeforeReload = trackFrames.size();
        QTest::keyClick(&window, Qt::Key_F5, Qt::ControlModifier | Qt::ShiftModifier);
        QTRY_COMPARE(changed.size(), 1);
        QVERIFY(ThemeLoader::lastError().isEmpty());
        QTRY_VERIFY(pianoFrames.size() > pianoBeforeReload &&
                    trackFrames.size() > tracksBeforeReload);
        QCOMPARE(themes->currentThemeId(), themeId);
        QCOMPARE(window.styleSheet(), originalStyle);
        QCOMPARE(piano->property("whiteKeyColor").value<QColor>(), originalWhiteKey);
        QCOMPARE(tracks->property("backgroundColor").value<QColor>(), originalBackground);
        verifyTimelines();
        if (QTest::currentTestFailed())
            return;
    }
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.context->m_appModel), model);
    QCOMPARE(appStatus->activeClipId.get(), clip->id());
    QVERIFY(!historyManager->canUndo());
    QVERIFY2(themes->applyTheme(ThemeIds::defaultThemeId()), qPrintable(ThemeLoader::lastError()));
    QTRY_COMPARE(piano->property("whiteKeyColor").value<QColor>(), darkPiano);
    QTRY_COMPARE(tracks->property("backgroundColor").value<QColor>(), darkTracks);
    verifyTimelines();
    if (QTest::currentTestFailed())
        return;

    QVERIFY(window.setTrackPanelScale(2, 1));
    QVERIFY(window.centerTrackPanelAt(1920, 0));
    QVERIFY(window.setPianoRollScale(3, 1));
    QVERIFY(window.centerPianoRollAt(10000, 30));
    trackController->setActiveClip(-1);
    QCOMPARE(appStatus->activeClipId.get(), -1);
    QVERIFY(window.setEditorPanelVisibility(true, false));
    QCoreApplication::processEvents();
    const QPoint openPosition(qRound((2880 - tracks->startTick()) * tracks->width() /
                                     (tracks->endTick() - tracks->startTick())),
                              qRound(0.6 * TracksEditorGlobal::trackHeight * tracks->scaleY() -
                                     tracks->logicalVisibleRect().top()));
    QVERIFY(tracks->rect().contains(openPosition));
    const auto clickedTick = tracks->startTick() + openPosition.x() *
                                                       (tracks->endTick() - tracks->startTick()) /
                                                       tracks->width();
    const auto beforeOpenFrame = pianoFrames.size();
    QTest::mouseClick(tracks, Qt::LeftButton, Qt::NoModifier, openPosition);
    QTest::mouseDClick(tracks, Qt::LeftButton, Qt::NoModifier, openPosition);
    QTest::mouseRelease(tracks, Qt::LeftButton, Qt::NoModifier, openPosition);
    QTRY_VERIFY(editor->isVisible() && piano->isVisible());
    QTRY_VERIFY(pianoFrames.size() > beforeOpenFrame);
    QCOMPARE(appStatus->activeClipId.get(), clip->id());
    const auto opened = window.captureEditorViewState();
    QVERIFY(opened.layout.bottomPanelVisible);
    QCOMPARE(opened.layout.bottomPanelPageId, QStringLiteral("ClipEditor"));
    QTRY_VERIFY(qAbs(window.captureEditorViewState().pianoRoll.centerTick - clickedTick) < 1.0);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.context->m_appModel), model);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(window.setPianoRollScale(1, 1));

    QVERIFY(editor->setEditMode(EditorViewGlobal::DrawNote));
    QVERIFY(editor->setRegionVisibility(true, false));
    window.raise();
    window.activateWindow();
    QTRY_VERIFY2(window.isActiveWindow(), qPrintable(windowInputState(window, *piano)));
    QTRY_VERIFY2(window.focusEditorRegion(EditorViewGlobal::Region::PianoRoll),
                 qPrintable(windowInputState(window, *piano)));
    const auto drawAndUndo = [&] {
        QVERIFY(window.centerPianoRollAt(1440, 60));
        QTRY_VERIFY(piano->height() > 0 && piano->width() > 0);
        QTest::mouseClick(piano, Qt::LeftButton, Qt::NoModifier, piano->rect().center());
        QCOMPARE(clip->notes().count(), 2);
        QVERIFY(runtime.history().undo(command()));
        QCOMPARE(TestSupport::projectSnapshot(*fixture.context->m_appModel), model);
        QVERIFY(!historyManager->canUndo());
    };
    drawAndUndo();
    if (QTest::currentTestFailed())
        return;

    auto *bottom = window.findChild<BottomPanelView *>();
    QVERIFY(bottom);
    const auto reattach = qScopeGuard([&] {
        if (bottom->isWindow())
            bottom->close();
    });
    auto *detachButton = bottom->titleBar()->findChild<Button *>("btnPanelDetach");
    QVERIFY(detachButton && detachButton->isVisible());
    const auto beforeDetach = runtime.documentVersion();
    const auto beforeDetachFrame = pianoFrames.size();
    QTest::mouseClick(detachButton, Qt::LeftButton);
    QTRY_VERIFY(bottom->isWindow() && bottom->isVisible());
    TestSupport::placeWindowOnScreen(*bottom, {1000, 650});
    bottom->raise();
    bottom->activateWindow();
    QTRY_VERIFY(bottom->isActiveWindow());
    QTRY_VERIFY(pianoFrames.size() > beforeDetachFrame);
    QCOMPARE(piano->window(), bottom);
    QCOMPARE(bottom->currentPageId(), QStringLiteral("ClipEditor"));
    QCOMPARE(runtime.documentVersion(), beforeDetach);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.context->m_appModel), model);
    QCOMPARE(appStatus->activeClipId.get(), clip->id());
    drawAndUndo();
    if (QTest::currentTestFailed())
        return;

    const auto beforeDock = runtime.documentVersion();
    const auto beforeDockFrame = pianoFrames.size();
    bottom->close();
    QTRY_VERIFY(!bottom->isWindow() && bottom->isVisible());
    window.raise();
    window.activateWindow();
    QTRY_VERIFY(window.isActiveWindow());
    QTRY_VERIFY(pianoFrames.size() > beforeDockFrame);
    QCOMPARE(piano->window(), &window);
    QCOMPARE(bottom->currentPageId(), QStringLiteral("ClipEditor"));
    QCOMPARE(runtime.documentVersion(), beforeDock);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.context->m_appModel), model);
    QCOMPARE(appStatus->activeClipId.get(), clip->id());
    drawAndUndo();
    if (QTest::currentTestFailed())
        return;
    QVERIFY(trackErrors.isEmpty() && pianoErrors.isEmpty());
}

void NativeDesktopTests::rhiPianoNavigationInputsReachTheActiveViewport() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    GuiDocumentFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.error));
    const auto backend = appOptions->developer()->editorRenderBackend;
    const auto tempoLaneVisible = appOptions->appearance()->showTempoLane;
    const auto signatureLaneVisible = appOptions->appearance()->showTimeSignatureLane;
    const auto restore = qScopeGuard([&] {
        appOptions->developer()->editorRenderBackend = backend;
        appOptions->appearance()->showTempoLane = tempoLaneVisible;
        appOptions->appearance()->showTimeSignatureLane = signatureLaneVisible;
        clipController->setClip(nullptr);
    });
    appOptions->developer()->editorRenderBackend =
        DeveloperOption::EditorRenderBackend::RhiExperimental;
    appOptions->appearance()->showTempoLane = true;
    appOptions->appearance()->showTimeSignatureLane = true;
    PianoRollView editor;
    auto *canvas = editor.findChild<PianoRollRhiWidget *>();
    auto *keyboard = editor.findChild<PianoKeyboardView *>();
    auto *timeline = editor.findChild<TimelineView *>();
    QVERIFY(canvas && keyboard && timeline);
    canvas->setApi(QRhiWidget::Api::Null);
    auto *clip = dynamic_cast<SingingClip *>(
        *fixture.context->m_appModel->tracks().first()->clips().begin());
    QVERIFY(clip);
    editor.setDataContext(clip);
    TestSupport::placeWindowOnScreen(editor, {1000, 500});
    editor.show();
    editor.activateWindow();
    QTRY_VERIFY(editor.isActiveWindow());
    QVERIFY(editor.setViewScale(1, 1));
    QVERIFY(editor.centerAt(1920, 60));
    QSignalSpy frames(canvas, &QRhiWidget::frameSubmitted);
    QSignalSpy failed(canvas, &QRhiWidget::renderFailed);
    canvas->update();
    QTRY_VERIFY(!frames.isEmpty());
    const auto before = fixture.context->m_coreRuntime->documentVersion();
    const auto model = TestSupport::projectSnapshot(*fixture.context->m_appModel);
    const QPoint gesturePosition(canvas->width() / 3, canvas->height() / 2);
    const auto tickAtPosition = [&] {
        return canvas->startTick() +
               (canvas->endTick() - canvas->startTick()) * gesturePosition.x() / canvas->width();
    };
    const auto anchorTick = tickAtPosition();
    const auto ticksPerPixel = (canvas->endTick() - canvas->startTick()) / canvas->width();
    const QPointingDevice touchpad(
        QStringLiteral("Test touchpad"), 1, QInputDevice::DeviceType::TouchPad,
        QPointingDevice::PointerType::Finger, QInputDevice::Capability::Position, 2, 0);
    const auto gestureGlobal = canvas->mapToGlobal(gesturePosition);
    QNativeGestureEvent gesture(Qt::ZoomNativeGesture, &touchpad, 2, gesturePosition,
                                gesturePosition, gestureGlobal, 0.25, {});
    QApplication::sendEvent(canvas, &gesture);
    QCOMPARE(canvas->scaleX(), 1.25);
    QCOMPARE(canvas->scaleY(), 1.25);
    QVERIFY(std::abs(tickAtPosition() - anchorTick) <= ticksPerPixel);
    const auto wheel = [](QWidget &target, int delta = 120) {
        const auto position = target.rect().center();
        QWheelEvent event(position, target.mapToGlobal(position), {}, {0, delta}, Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        event.setAccepted(false);
        QApplication::sendEvent(&target, &event);
        QVERIFY(event.isAccepted());
    };
    const auto horizontal = canvas->scaleX();
    wheel(*timeline);
    QTRY_VERIFY(canvas->scaleX() > horizontal);
    const auto vertical = canvas->scaleY();
    wheel(*keyboard);
    QTRY_VERIFY(canvas->scaleY() > vertical);
    const auto centerKey = canvas->centerKeyIndex();
    wheel(*canvas);
    QTRY_VERIFY(canvas->centerKeyIndex() != centerKey);
    const auto lanes = editor.findChildren<InfoLaneView *>();
    QVERIFY(!lanes.isEmpty());
    for (auto *lane : lanes) {
        QVERIFY(lane->isVisible());
        const auto previousKey = canvas->centerKeyIndex();
        const auto previousTick = canvas->startTick();
        const QPointF previousScale(canvas->scaleX(), canvas->scaleY());
        wheel(*lane);
        QTRY_VERIFY(canvas->centerKeyIndex() != previousKey);
        QCOMPARE(canvas->startTick(), previousTick);
        QCOMPARE(QPointF(canvas->scaleX(), canvas->scaleY()), previousScale);
    }
    const auto bars = canvas->findChildren<OverlayScrollBar *>();
    for (const auto orientation : {Qt::Horizontal, Qt::Vertical}) {
        const auto found = std::find_if(bars.begin(), bars.end(), [&](const auto *bar) {
            return bar->orientation() == orientation;
        });
        QVERIFY(found != bars.end());
        auto *bar = *found;
        QTRY_VERIFY(bar->isVisible() && bar->maximum() > 0);
        const auto barValue = bar->value();
        const auto previousTick = canvas->startTick();
        const auto previousKey = canvas->centerKeyIndex();
        wheel(*bar, -120);
        QTRY_VERIFY(bar->value() > barValue);
        // Scrollbar offsets have integer logical-pixel precision.
        if (bar->orientation() == Qt::Horizontal) {
            QTRY_VERIFY(canvas->startTick() > previousTick);
            QVERIFY(std::abs(canvas->centerKeyIndex() - previousKey) *
                        ClipEditorGlobal::noteHeight * canvas->scaleY() <=
                    1.0);
        } else {
            QTRY_VERIFY(canvas->centerKeyIndex() < previousKey);
            QVERIFY(std::abs(canvas->startTick() - previousTick) <=
                    (canvas->endTick() - canvas->startTick()) / canvas->width());
        }
    }
    QVERIFY(editor.centerAt(1920, 60));
    const auto touchStartTick = canvas->startTick();
    const auto touchCenterKey = canvas->centerKeyIndex();
    const auto press = canvas->rect().center();
    const auto destination = press - QPoint(40, 20);
    auto *touchDevice = QTest::createTouchDevice();
    auto touchSequence = QTest::touchEvent(canvas, touchDevice, false);
    const auto stopPointer = qScopeGuard([&] {
        QEvent deactivate(QEvent::WindowDeactivate);
        QApplication::sendEvent(canvas, &deactivate);
    });
    touchSequence.press(0, press).commit();
    touchSequence.move(0, destination).commit();
    QVERIFY(canvas->startTick() > touchStartTick);
    QVERIFY(canvas->centerKeyIndex() < touchCenterKey);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    touchSequence.release(0, destination).commit();
    const auto previousFrame = frames.size();
    canvas->update();
    QTRY_VERIFY(frames.size() > previousFrame);
    QCOMPARE(fixture.context->m_coreRuntime->documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.context->m_appModel), model);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(failed.isEmpty());
}

void NativeDesktopTests::rhiNoteDrawingCommitsAndUndoUpdatesInteraction_data() {
    QTest::addColumn<bool>("doubleClick");
    QTest::addColumn<bool>("touch");
    QTest::newRow("draw-tool") << false << false;
    QTest::newRow("select-tool-double-click") << true << false;
    QTest::newRow("touch-draw-tool") << false << true;
}

void NativeDesktopTests::rhiNoteDrawingCommitsAndUndoUpdatesInteraction() {
    QFETCH(bool, doubleClick);
    QFETCH(bool, touch);
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
    canvas.setEditMode(doubleClick ? ClipEditorGlobal::Select : ClipEditorGlobal::DrawNote);
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
    auto *touchDevice = QTest::createTouchDevice();
    auto touchSequence = QTest::touchEvent(&canvas, touchDevice, false);
    const auto stopPointer = qScopeGuard([&] {
        QEvent deactivate(QEvent::WindowDeactivate);
        QApplication::sendEvent(&canvas, &deactivate);
    });
    const auto before = runtime.documentVersion();
    const auto beforePreviewFrame = submitted.size();
    if (touch) {
        touchSequence.press(0, press).commit();
        touchSequence.move(0, release).commit();
    } else {
        if (doubleClick)
            QTest::mouseDClick(&canvas, Qt::LeftButton, Qt::NoModifier, press);
        else
            QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, press);
        QMouseEvent move(QEvent::MouseMove, QPointF(release), QPointF(canvas.mapToGlobal(release)),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&canvas, &move);
    }
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
    if (touch)
        touchSequence.release(0, release).commit();
    else
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

    QCOMPARE(canvas.focusVisibility(focus), HistoryFocusVisibility::Visible);
    QVERIFY(canvas.centerAt(1920, 90));
    QCOMPARE(canvas.focusVisibility(focus), HistoryFocusVisibility::ScrollRequired);
    QVERIFY(canvas.revealFocus(focus, false));
    QCOMPARE(canvas.focusVisibility(focus), HistoryFocusVisibility::Visible);
    QCOMPARE(clip->notes().count(), 0);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(canvas.centerAt(1920, 60));

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
    const auto original = TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
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
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), original);
    QTest::keyClick(&canvas, Qt::Key_Escape);
    QTest::mouseRelease(canvas.windowHandle(), Qt::LeftButton, Qt::NoModifier, edge);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
    QTest::qWait(80);
    QCOMPARE(canvas.startTick(), initialStart);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), original);
    QVERIFY(!historyManager->canUndo());
}

void NativeDesktopTests::rhiNoteMoveCanBeCanceledAndThenCommitted_data() {
    QTest::addColumn<bool>("platformRenderer");
    QTest::newRow("null-renderer") << false;
    QTest::newRow("platform-renderer") << true;
}

void NativeDesktopTests::rhiNoteMoveCanBeCanceledAndThenCommitted() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    QFETCH(bool, platformRenderer);
    const auto restoreSoftware = preferSoftwareRhi(platformRenderer);
    if (platformRenderer && !platformRhiAvailable())
        QSKIP("No graphics backend is available for framebuffer validation");
    ExistingRhiNoteFixture fixture;
    fixture.initialize(38400, platformRenderer ? platformRhiApi() : QRhiWidget::Api::Null);
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
    QTest::keyClick(&canvas, Qt::Key_Escape);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, release);
    QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(note->localStart(), 480);
    QCOMPARE(note->keyIndex(), 60);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    QImage originalFrame;
    QRect originalRegion;
    QRect movedRegion;
    if (platformRenderer) {
        const auto regionFor = [&](const QImage &frame, const QPoint &point) {
            const QPoint pixel(qRound(point.x() * double(frame.width()) / canvas.width()),
                               qRound(point.y() * double(frame.height()) / canvas.height()));
            return QRect(pixel - QPoint(1, 1), QSize(3, 3));
        };
        const auto originalSize = canvas.size();
        const auto originalView = canvas.viewState();
        const auto originalModel = TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
        const auto selected = appStatus->selectedNotes.get();
        const auto beforeResize = canvas.grabFramebuffer();
        QVERIFY(!beforeResize.isNull());
        const auto beforeResizeNote =
            beforeResize.copy(regionFor(beforeResize, fixture.pointFor(840, 60)));
        canvas.resize(originalSize.width() - 180, originalSize.height() - 120);
        fixture.waitForFrame();
        if (QTest::currentTestFailed())
            return;
        const auto resized = canvas.grabFramebuffer();
        QVERIFY(!resized.isNull());
        QCOMPARE(resized.size(), QSize(qRound(canvas.width() * canvas.devicePixelRatioF()),
                                       qRound(canvas.height() * canvas.devicePixelRatioF())));
        QVERIFY(resized.size() != beforeResize.size());
        const auto resizedNoteRegion = regionFor(resized, fixture.pointFor(840, 60));
        QVERIFY(resized.rect().contains(resizedNoteRegion));
        QCOMPARE(resized.copy(resizedNoteRegion), beforeResizeNote);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, fixture.pointFor(720, 60));
        QCOMPARE(appStatus->selectedNotes.get(), QList<int>{fixture.noteId});
        canvas.resize(originalSize);
        QVERIFY(canvas.setViewScale(originalView.horizontalScale, originalView.verticalScale));
        QVERIFY(canvas.centerAt(originalView.centerTick, originalView.centerKeyIndex));
        fixture.waitForFrame();
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(appStatus->selectedNotes.get(), selected);
        QCOMPARE(fixture.runtime().documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), originalModel);
        QVERIFY(!historyManager->canUndo());
        originalFrame = canvas.grabFramebuffer();
        QVERIFY(!originalFrame.isNull());
        QCOMPARE(originalFrame.size(), beforeResize.size());
        originalRegion = regionFor(originalFrame, fixture.pointFor(840, 60));
        movedRegion = regionFor(originalFrame, fixture.pointFor(1320, 62));
        QVERIFY(originalFrame.rect().contains(originalRegion));
        QVERIFY(originalFrame.rect().contains(movedRegion));
        QCOMPARE(originalFrame.copy(originalRegion), beforeResize.copy(originalRegion));
        QCOMPARE(originalFrame.copy(movedRegion), beforeResize.copy(movedRegion));
    }
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, press);
    fixture.moveTo(release);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, release);
    QCOMPARE(note->localStart(), 960);
    QCOMPARE(note->keyIndex(), 62);
    QCOMPARE(note->length(), 480);
    QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(fixture.runtime().documentVersion().revision, before.revision + 1);
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    QImage movedFrame;
    if (platformRenderer) {
        QTRY_VERIFY(!(movedFrame = canvas.grabFramebuffer()).isNull() &&
                    movedFrame.copy(originalRegion) != originalFrame.copy(originalRegion));
        QVERIFY(movedFrame.copy(movedRegion) != originalFrame.copy(movedRegion));
    }
    QVERIFY(fixture.runtime().history().undo(fixture.command()));
    QCOMPARE(note->localStart(), 480);
    QCOMPARE(note->keyIndex(), 60);
    QVERIFY(!historyManager->canUndo());
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    if (platformRenderer) {
        QImage restoredFrame;
        QTRY_VERIFY((restoredFrame = canvas.grabFramebuffer()).copy(originalRegion) ==
                    originalFrame.copy(originalRegion));
        QCOMPARE(restoredFrame.copy(movedRegion), originalFrame.copy(movedRegion));
    }
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
    const auto beforeFollowing = TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
    const auto beforeJump = canvas.endTick();
    const auto distantPosition = beforeJump + 2 * (beforeJump - canvas.startTick());
    QVERIFY(distantPosition < fixture.clip->length());
    canvas.setPlaybackPosition(distantPosition);
    QTRY_VERIFY(canvas.startTick() > beforeJump && canvas.startTick() <= distantPosition &&
                canvas.endTick() >= distantPosition);
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    canvas.setPlaybackPosition(480);
    QTRY_VERIFY(canvas.startTick() <= 480 && canvas.endTick() >= 480);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), beforeFollowing);
    QCOMPARE(fixture.runtime().documentVersion(), afterEditing);
    QCOMPARE(note->localStart(), 480);
    QVERIFY(!historyManager->canUndo());
}

void NativeDesktopTests::rhiNoteResizeUndoRestoresTheHitRegion_data() {
    QTest::addColumn<bool>("leftEdge");
    QTest::addColumn<bool>("touch");
    QTest::addColumn<bool>("joint");
    QTest::addColumn<bool>("cancel");
    QTest::newRow("left-edge") << true << false << false << false;
    QTest::newRow("right-edge") << false << false << false << false;
    QTest::newRow("touch-left-handle") << true << true << false << false;
    QTest::newRow("touch-right-handle") << false << true << false << false;
    QTest::newRow("joint-left") << true << false << true << false;
    QTest::newRow("joint-right") << false << false << true << false;
    QTest::newRow("joint-cancel") << false << false << true << true;
}

void NativeDesktopTests::rhiNoteResizeUndoRestoresTheHitRegion() {
    QFETCH(bool, leftEdge);
    QFETCH(bool, touch);
    QFETCH(bool, joint);
    QFETCH(bool, cancel);
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    ExistingRhiNoteFixture fixture;
    fixture.initialize();
    if (QTest::currentTestFailed())
        return;
    auto &canvas = *fixture.canvas;
    auto *note = fixture.clip->findNoteById(fixture.noteId);
    QVERIFY(note);
    const Note *neighbor = nullptr;
    if (joint) {
        Automation::NoteDraftDto draft;
        draft.localStart = leftEdge ? 0 : 960;
        draft.length = 480;
        draft.keyIndex = 64;
        draft.lyric = QStringLiteral("li");
        draft.language = QStringLiteral("eng");
        const auto inserted = fixture.runtime().notes().insertNotes(
            fixture.command(), Automation::ClipId(fixture.clip->id()), {draft});
        QVERIFY(inserted && !inserted.get().affectedObjects.isEmpty());
        neighbor = fixture.clip->findNoteById(inserted.get().affectedObjects.first().value);
        QVERIFY(neighbor);
        fixture.waitForFrame();
        if (QTest::currentTestFailed())
            return;
        historyManager->reset();
    }
    auto *touchDevice = QTest::createTouchDevice();
    auto touchSequence = QTest::touchEvent(&canvas, touchDevice, false);
    const auto cancelPointer = qScopeGuard([&] {
        QEvent deactivate(QEvent::WindowDeactivate);
        QApplication::sendEvent(&canvas, &deactivate);
    });
    if (touch) {
        const auto center = fixture.pointFor(720, 60);
        touchSequence.press(0, center).commit();
        touchSequence.release(0, center).commit();
        QCOMPARE(appStatus->selectedNotes.get(), QList<int>{fixture.noteId});
        fixture.waitForFrame();
        if (QTest::currentTestFailed())
            return;
    }
    const auto inset = touch ? -5 : 2;
    const auto edgeOffset = QPoint(leftEdge ? inset : -inset, 0);
    const auto press = fixture.pointFor(leftEdge ? 480 : 960, 60) + edgeOffset;
    // Left resizing snaps down, so finish inside the requested grid cell.
    const auto release = fixture.pointFor(leftEdge ? 240 : 1200, 60) + QPoint(leftEdge ? 2 : -2, 0);
    QVERIFY(canvas.rect().contains(press));
    QVERIFY(canvas.rect().contains(release));
    const auto before = fixture.runtime().documentVersion();
    const auto beforeProject = TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
    if (touch) {
        touchSequence.press(0, press).commit();
        touchSequence.move(0, release).commit();
    } else {
        QTest::mousePress(&canvas, Qt::LeftButton, joint ? Qt::ShiftModifier : Qt::NoModifier,
                          press);
        fixture.moveTo(release);
    }
    QTRY_COMPARE(appStatus->pianoRollNoteEditPreview.get().size(), joint ? 2 : 1);
    const auto previews = appStatus->pianoRollNoteEditPreview.get();
    const auto previewFor = [&](int noteId) {
        for (const auto &preview : previews)
            if (preview.id == noteId)
                return preview;
        return AppStatus::NoteEditPreview{};
    };
    const auto preview = previewFor(fixture.noteId);
    QCOMPARE(preview.id, fixture.noteId);
    QCOMPARE(preview.rStart, leftEdge ? 240 : 480);
    QCOMPARE(preview.length, 720);
    if (neighbor) {
        const auto neighborPreview = previewFor(neighbor->id());
        QCOMPARE(neighborPreview.id, neighbor->id());
        QCOMPARE(neighborPreview.rStart, leftEdge ? 0 : 1200);
        QCOMPARE(neighborPreview.length, 240);
        QCOMPARE(neighborPreview.keyIndex, 64);
        QCOMPARE(neighbor->localStart(), leftEdge ? 0 : 960);
        QCOMPARE(neighbor->length(), 480);
    }
    QCOMPARE(note->length(), 480);
    QCOMPARE(note->localStart(), 480);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), beforeProject);
    if (cancel) {
        QEvent deactivate(QEvent::WindowDeactivate);
        QApplication::sendEvent(&canvas, &deactivate);
    }
    if (touch)
        touchSequence.release(0, release).commit();
    else
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, release);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
    if (cancel) {
        QCOMPARE(fixture.runtime().documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), beforeProject);
        QVERIFY(!historyManager->canUndo());
        fixture.waitForFrame();
        return;
    }
    QCOMPARE(note->length(), 720);
    QCOMPARE(note->localStart(), leftEdge ? 240 : 480);
    if (neighbor) {
        QCOMPARE(neighbor->localStart(), leftEdge ? 0 : 1200);
        QCOMPARE(neighbor->length(), 240);
    }
    QCOMPARE(fixture.runtime().documentVersion().revision, before.revision + 1);
    const auto committedProject = TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    const auto extendedArea = fixture.pointFor(leftEdge ? 360 : 1080, 60);
    const auto vacatedNeighborArea = fixture.pointFor(leftEdge ? 360 : 1080, 64);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, extendedArea);
    QCOMPARE(appStatus->selectedNotes.get(), QList<int>{fixture.noteId});
    if (neighbor) {
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, vacatedNeighborArea);
        QVERIFY(appStatus->selectedNotes.get().isEmpty());
    }
    QVERIFY(fixture.runtime().history().undo(fixture.command()));
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), beforeProject);
    QCOMPARE(note->length(), 480);
    QCOMPARE(note->localStart(), 480);
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    if (neighbor) {
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, vacatedNeighborArea);
        QCOMPARE(appStatus->selectedNotes.get(), QList<int>{neighbor->id()});
    }
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, extendedArea);
    QVERIFY(appStatus->selectedNotes.get().isEmpty());
    QVERIFY(!historyManager->canUndo());
    QVERIFY(historyManager->canRedo());
    if (joint) {
        QVERIFY(fixture.runtime().history().redo(fixture.command()));
        QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), committedProject);
        fixture.waitForFrame();
        if (QTest::currentTestFailed())
            return;
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, vacatedNeighborArea);
        QVERIFY(appStatus->selectedNotes.get().isEmpty());
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, extendedArea);
        QCOMPARE(appStatus->selectedNotes.get(), QList<int>{fixture.noteId});
    }
}

void NativeDesktopTests::rhiPitchAnchorInsertionAndCanceledDragUseTheRealEditor_data() {
    QTest::addColumn<bool>("crossCurve");
    QTest::newRow("within-curve") << false;
    QTest::newRow("across-curves") << true;
}

void NativeDesktopTests::rhiAnchorSelectionMovesTheGroupAtomically_data() {
    QTest::addColumn<bool>("acrossCurves");
    QTest::addColumn<bool>("edgeScroll");
    QTest::newRow("within-curve") << false << false;
    QTest::newRow("across-curves") << true << false;
    QTest::newRow("edge-scroll") << false << true;
}

void NativeDesktopTests::rhiAnchorSelectionMovesTheGroupAtomically() {
    QFETCH(bool, acrossCurves);
    QFETCH(bool, edgeScroll);
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    ExistingRhiNoteFixture fixture;
    fixture.initialize(edgeScroll ? 38400 : 3840);
    if (QTest::currentTestFailed())
        return;
    auto &canvas = *fixture.canvas;
    Automation::CurveDraftDto first;
    first.type = Automation::CurveDraftDto::Type::Anchor;
    first.nodes = {
        {480,                       6000,                       AnchorNode::Hermite},
        {960,                       6300,                       AnchorNode::Hermite},
        {edgeScroll ? 10000 : 1440, acrossCurves ? 6400 : 6000, AnchorNode::None   }
    };
    QList<Automation::CurveDraftDto> curves{first};
    if (acrossCurves) {
        auto second = first;
        second.nodes = {
            {1920, 6000, AnchorNode::Hermite},
            {2400, 6300, AnchorNode::Hermite},
            {2880, 6400, AnchorNode::None   }
        };
        curves.append(second);
    }
    QVERIFY(fixture.runtime().parameters().replaceParameter(
        fixture.command(), Automation::ClipId(fixture.clip->id()), ParamInfo::Pitch, Param::Edited,
        curves));
    auto *pitch = fixture.clip->params.getParamByName(ParamInfo::Pitch);
    QVERIFY(pitch);
    const auto anchors = [&] {
        QMap<int, QPair<int, int>> result;
        for (const auto *curve : pitch->curves(Param::Edited)) {
            const auto *anchorCurve = dynamic_cast<const AnchorCurve *>(curve);
            if (!anchorCurve)
                continue;
            for (const auto *node : anchorCurve->nodes())
                result.insert(node->id(), {node->pos(), node->value()});
        }
        return result;
    };
    const auto initial = anchors();
    QList<int> selected;
    for (auto it = initial.cbegin(); it != initial.cend(); ++it)
        if (it.value().first == 480 || it.value().first == (acrossCurves ? 1920 : 960))
            selected.append(it.key());
    QCOMPARE(selected.size(), 2);
    canvas.setEditMode(ClipEditorGlobal::EditPitchAnchor);
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    historyManager->reset();
    const auto before = fixture.runtime().documentVersion();
    const auto beforeModel = TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
    const auto selectGroup = [&] {
        if (edgeScroll) {
            QVERIFY(canvas.centerAt(1920, 60));
            fixture.waitForFrame();
            if (QTest::currentTestFailed())
                return;
        }
        const auto start = fixture.pointFor(360, acrossCurves ? 60.5 : 64);
        const auto end = fixture.pointFor(acrossCurves ? 2040 : 1200, acrossCurves ? 59.5 : 59);
        QVERIFY(canvas.rect().contains(start) && canvas.rect().contains(end));
        fixture.moveTo(start);
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(&canvas, end);
        fixture.waitForFrame(QEventLoop::ExcludeUserInputEvents);
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(fixture.runtime().documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), beforeModel);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, end);
        QCoreApplication::processEvents();
        fixture.waitForFrame();
    };
    const auto press = fixture.pointFor(480, 60);
    auto release = fixture.pointFor(720, 61);
    if (edgeScroll)
        release.setX(canvas.width() - 2);
    const auto releasePointer = [&] {
        if (edgeScroll)
            QTest::mouseRelease(canvas.windowHandle(), Qt::LeftButton, Qt::NoModifier, release);
        else
            QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, release);
    };
    const auto cancelPointer = qScopeGuard([&] {
        if (edgeScroll) {
            QEvent deactivate(QEvent::WindowDeactivate);
            QApplication::sendEvent(&canvas, &deactivate);
            releasePointer();
        }
    });
    const auto dragGroup = [&] {
        const auto startTick = canvas.startTick();
        fixture.moveTo(press);
        if (edgeScroll) {
            QVERIFY(canvas.windowHandle());
            // The scroll timer checks Qt's application-wide mouse-button state.
            QTest::mousePress(canvas.windowHandle(), Qt::LeftButton, Qt::NoModifier, press);
            QCursor::setPos(canvas.mapToGlobal(release));
            QTest::mouseMove(canvas.windowHandle(), release);
            QVERIFY(QGuiApplication::mouseButtons().testFlag(Qt::LeftButton));
        } else {
            QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, press);
            QTest::mouseMove(&canvas, release);
        }
        fixture.waitForFrame(QEventLoop::ExcludeUserInputEvents);
        if (edgeScroll) {
            QTRY_VERIFY(canvas.startTick() > startTick);
            const auto scrollingTick = canvas.startTick();
            QTRY_VERIFY(canvas.startTick() > scrollingTick);
        }
    };
    selectGroup();
    if (QTest::currentTestFailed())
        return;
    dragGroup();
    if (QTest::currentTestFailed())
        return;
    QVERIFY(editSessionManager->hasActiveTransaction());
    QCOMPARE(anchors(), initial);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), beforeModel);
    QTest::keyClick(&canvas, Qt::Key_Escape);
    releasePointer();
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), beforeModel);
    QVERIFY(!historyManager->canUndo());
    if (edgeScroll) {
        const auto stoppedTick = canvas.startTick();
        QTest::qWait(80);
        QCOMPARE(canvas.startTick(), stoppedTick);
    }
    selectGroup();
    if (QTest::currentTestFailed())
        return;
    dragGroup();
    if (QTest::currentTestFailed())
        return;
    releasePointer();
    QCoreApplication::processEvents();
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    QVERIFY(!editSessionManager->hasActiveTransaction());
    const auto moved = anchors();
    QCOMPARE(moved.keys(), initial.keys());
    const auto tickPerPixel = (canvas.endTick() - canvas.startTick()) / canvas.width();
    const auto delta = moved.value(selected.first()).first - initial.value(selected.first()).first;
    if (edgeScroll)
        QVERIFY(delta > (release.x() - press.x()) * tickPerPixel);
    else
        QVERIFY(qAbs(delta - 240) <= tickPerPixel);
    for (auto it = initial.cbegin(); it != initial.cend(); ++it) {
        const auto actual = moved.value(it.key());
        if (selected.contains(it.key())) {
            QCOMPARE(actual.first - it.value().first, delta);
            QCOMPARE(actual.second - it.value().second, 100);
        } else {
            QCOMPARE(actual, it.value());
        }
    }
    QCOMPARE(fixture.runtime().documentVersion().revision, before.revision + 1);
    if (edgeScroll) {
        const auto stoppedTick = canvas.startTick();
        QTest::qWait(80);
        QCOMPARE(canvas.startTick(), stoppedTick);
    }
    QVERIFY(fixture.runtime().history().undo(fixture.command()));
    QCOMPARE(anchors(), initial);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), beforeModel);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(fixture.runtime().history().redo(fixture.command()));
    QCOMPARE(anchors(), moved);
    fixture.waitForFrame();
}

void NativeDesktopTests::rhiPitchAnchorInsertionAndCanceledDragUseTheRealEditor() {
    QFETCH(bool, crossCurve);
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
    QList<Automation::CurveDraftDto> curves{draft};
    if (crossCurve) {
        auto target = draft;
        target.nodes = {
            {1920, 6000, AnchorNode::Hermite},
            {2880, 6000, AnchorNode::None   }
        };
        curves.append(target);
    }
    QVERIFY(fixture.runtime().parameters().replaceParameter(
        fixture.command(), Automation::ClipId(fixture.clip->id()), ParamInfo::Pitch, Param::Edited,
        curves));
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
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    const auto beforeAppend = TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
    const auto appendPosition = fixture.pointFor(1680, 61);
    QTest::mouseMove(canvas.windowHandle(), appendPosition);
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), beforeAppend);
    QCOMPARE(anchorCurve()->nodes().toList().last()->interpMode(), AnchorNode::None);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, appendPosition);
    QCOMPARE(anchorCurve()->nodes().count(), 4);
    const auto appendedNodes = anchorCurve()->nodes().toList();
    QVERIFY(qAbs(appendedNodes.last()->pos() - 1680) <= tickPerPixel);
    QCOMPARE(appendedNodes.last()->value(), 6100);
    QCOMPARE(appendedNodes.last()->interpMode(), AnchorNode::None);
    QCOMPARE(appendedNodes.at(2)->interpMode(), AnchorNode::Hermite);
    QVERIFY(fixture.runtime().history().undo(fixture.command()));
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), beforeAppend);
    inserted = anchorCurve()->nodes().toList().at(1);
    const auto *historyEntry = historyManager->nextUndoEntry();
    const auto beforeCancel = fixture.runtime().documentVersion();
    const auto press = fixture.pointFor(insertedTick, 62);
    const auto targetTick = crossCurve ? 2400 : insertedTick + 240;
    const auto release = fixture.pointFor(targetTick, 61);
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, press);
    fixture.moveTo(release);
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    QVERIFY(editSessionManager->hasActiveTransaction());
    QCOMPARE(inserted->pos(), insertedTick);
    QCOMPARE(inserted->value(), 6200);
    QTest::keyClick(&canvas, Qt::Key_Escape);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, release);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(fixture.runtime().documentVersion(), beforeCancel);
    QCOMPARE(historyManager->nextUndoEntry(), historyEntry);
    const auto *restored = anchorCurve()->nodes().toList().at(1);
    QCOMPARE(restored->id(), insertedId);
    QCOMPARE(restored->pos(), insertedTick);
    QCOMPARE(restored->value(), 6200);
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    if (crossCurve) {
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, press);
        fixture.moveTo(release);
        fixture.waitForFrame();
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), beforeAppend);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, release);
        QVERIFY(!editSessionManager->hasActiveTransaction());
        QCOMPARE(anchorCurve()->nodes().count(), 2);
        const auto *target =
            dynamic_cast<const AnchorCurve *>(pitch->curves(Param::Edited).value(1));
        QVERIFY(target);
        QCOMPARE(target->nodes().count(), 3);
        const auto *moved = target->nodes().toList().at(1);
        QCOMPARE(moved->id(), insertedId);
        QVERIFY(qAbs(moved->pos() - targetTick) <= tickPerPixel);
        QCOMPARE(moved->value(), 6100);
        QVERIFY(fixture.runtime().history().undo(fixture.command()));
        QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), beforeAppend);

        QList<int> nodeIds;
        for (const auto *curve : pitch->curves(Param::Edited)) {
            const auto *anchors = dynamic_cast<const AnchorCurve *>(curve);
            QVERIFY(anchors);
            for (const auto *node : anchors->nodes())
                nodeIds.append(node->id());
        }
        const auto beforeMerge = fixture.runtime().documentVersion();
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, fixture.pointFor(1440, 60));
        const auto nextEndpoint = fixture.pointFor(1920, 60);
        QTest::mouseMove(canvas.windowHandle(), nextEndpoint);
        fixture.waitForFrame();
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), beforeAppend);
        QCOMPARE(fixture.runtime().documentVersion(), beforeMerge);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, nextEndpoint);
        QCOMPARE(pitch->curves(Param::Edited).size(), 1);
        QVERIFY(anchorCurve());
        const auto mergedNodes = anchorCurve()->nodes().toList();
        QCOMPARE(mergedNodes.size(), nodeIds.size());
        const QList<int> ticks{480, insertedTick, 1440, 1920, 2880};
        const QList<int> values{6000, 6200, 6000, 6000, 6000};
        for (int index = 0; index < mergedNodes.size(); ++index) {
            QCOMPARE(mergedNodes[index]->id(), nodeIds[index]);
            QCOMPARE(mergedNodes[index]->pos(), ticks[index]);
            QCOMPARE(mergedNodes[index]->value(), values[index]);
        }
        QCOMPARE(fixture.runtime().documentVersion().revision, beforeMerge.revision + 1);
        QVERIFY(fixture.runtime().history().undo(fixture.command()));
        QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), beforeAppend);
    }
    QVERIFY(fixture.runtime().history().undo(fixture.command()));
    QVERIFY(anchorCurve());
    QCOMPARE(anchorCurve()->nodes().count(), 2);
    QCOMPARE(anchorCurve()->nodes().toList().first()->pos(), 480);
    QCOMPARE(anchorCurve()->nodes().toList().last()->pos(), 1440);
    QVERIFY(!historyManager->canUndo());
    fixture.waitForFrame();
}

void NativeDesktopTests::rhiNoteEraseStrokeCancelsAndCommitsAtomically_data() {
    QTest::addColumn<bool>("penEraser");
    QTest::addColumn<bool>("edgeScroll");
    QTest::newRow("mouse") << false << false;
    QTest::newRow("pen-eraser-under-draw-tool") << true << false;
    QTest::newRow("mouse-edge-scroll") << false << true;
}

void NativeDesktopTests::rhiNoteEraseStrokeCancelsAndCommitsAtomically() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    QFETCH(bool, penEraser);
    QFETCH(bool, edgeScroll);
    ExistingRhiNoteFixture fixture;
    fixture.initialize(edgeScroll ? 38400 : 3840);
    if (QTest::currentTestFailed())
        return;
    fixture.addSecondNote();
    if (QTest::currentTestFailed())
        return;
    auto &canvas = *fixture.canvas;
    const auto secondCenterTick =
        edgeScroll ? static_cast<int>(std::ceil(canvas.endTick())) + 360 : 1440;
    if (edgeScroll) {
        QVERIFY(fixture.runtime().notes().moveNotes(
            fixture.command(), Automation::ClipId(fixture.clip->id()),
            {Automation::NoteId(fixture.secondNoteId)}, secondCenterTick - 1440, 0));
        fixture.waitForFrame();
        if (QTest::currentTestFailed())
            return;
        QVERIFY(!canvas.rect().contains(fixture.pointFor(secondCenterTick, 62)));
        historyManager->reset();
    }
    canvas.setEditMode(penEraser ? ClipEditorGlobal::DrawNote : ClipEditorGlobal::EraseNote);
    const auto first = fixture.pointFor(720, 60);
    auto second = fixture.pointFor(1440, 62);
    if (edgeScroll)
        second.setX(canvas.width() - 2);
    QVERIFY(canvas.rect().contains(first) && canvas.rect().contains(second));
    const QList<int> erased{fixture.noteId, fixture.secondNoteId};
    const auto before = fixture.runtime().documentVersion();
    const auto beforeModel = TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
    const QPointingDevice pen(
        QStringLiteral("Fixture note eraser"), 1004, QInputDevice::DeviceType::Stylus,
        QPointingDevice::PointerType::Eraser,
        QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 2);
    const auto cancelPointer = qScopeGuard([&] {
        QEvent deactivate(QEvent::WindowDeactivate);
        QApplication::sendEvent(&canvas, &deactivate);
        if (edgeScroll)
            QTest::mouseRelease(canvas.windowHandle(), Qt::LeftButton, Qt::NoModifier, second);
    });
    const auto press = [&] {
        if (edgeScroll) {
            QVERIFY(canvas.centerAt(1920, 60));
            fixture.waitForFrame();
            if (QTest::currentTestFailed())
                return;
            QTest::mousePress(canvas.windowHandle(), Qt::LeftButton, Qt::NoModifier, first);
            QVERIFY(QGuiApplication::mouseButtons().testFlag(Qt::LeftButton));
            return;
        }
        if (penEraser)
            QVERIFY(TestSupport::sendTabletEvent(canvas, pen, QEvent::TabletPress, first, 0.7,
                                                 Qt::LeftButton, Qt::LeftButton));
        else
            QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, first);
    };
    const auto move = [&] {
        if (edgeScroll) {
            const auto startTick = canvas.startTick();
            QCursor::setPos(canvas.mapToGlobal(second));
            QTest::mouseMove(canvas.windowHandle(), second);
            QTRY_VERIFY(canvas.startTick() > startTick);
            QTRY_VERIFY(appStatus->pianoRollNoteErasePreview.get().contains(fixture.secondNoteId));
            return;
        }
        if (penEraser)
            QVERIFY(TestSupport::sendTabletEvent(canvas, pen, QEvent::TabletMove, second, 0.7,
                                                 Qt::NoButton, Qt::LeftButton));
        else
            fixture.moveTo(second);
    };
    const auto release = [&] {
        if (edgeScroll) {
            QTest::mouseRelease(canvas.windowHandle(), Qt::LeftButton, Qt::NoModifier, second);
            const auto stoppedTick = canvas.startTick();
            QTest::qWait(80);
            QCOMPARE(canvas.startTick(), stoppedTick);
            return;
        }
        if (penEraser)
            QVERIFY(TestSupport::sendTabletEvent(canvas, pen, QEvent::TabletRelease, second, 0,
                                                 Qt::LeftButton, Qt::NoButton));
        else
            QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, second);
    };
    press();
    move();
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(appStatus->pianoRollNoteErasePreview.get(), erased);
    QVERIFY(editSessionManager->hasActiveTransaction());
    QCOMPARE(fixture.clip->notes().count(), 2);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QTest::keyClick(&canvas, Qt::Key_Escape);
    release();
    QVERIFY(appStatus->pianoRollNoteErasePreview.get().isEmpty());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(fixture.clip->notes().count(), 2);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), beforeModel);
    QVERIFY(!historyManager->canUndo());

    press();
    move();
    release();
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(fixture.clip->notes().count(), 0);
    QVERIFY(appStatus->pianoRollNoteErasePreview.get().isEmpty());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(fixture.runtime().documentVersion().revision, before.revision + 1);
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    historyManager->undo();
    QCOMPARE(fixture.clip->notes().count(), 2);
    QVERIFY(fixture.clip->findNoteById(fixture.noteId));
    QVERIFY(fixture.clip->findNoteById(fixture.secondNoteId));
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), beforeModel);
    QVERIFY(!historyManager->canUndo());
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    canvas.setEditMode(ClipEditorGlobal::Select);
    if (edgeScroll) {
        QVERIFY(canvas.centerAt(1920, 60));
        fixture.waitForFrame();
        if (QTest::currentTestFailed())
            return;
    }
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, first);
    QCOMPARE(appStatus->selectedNotes.get(), QList<int>{fixture.noteId});
    if (edgeScroll) {
        QVERIFY(canvas.centerAt(secondCenterTick, 62));
        fixture.waitForFrame();
        if (QTest::currentTestFailed())
            return;
        second = fixture.pointFor(secondCenterTick, 62);
    }
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, second);
    QCOMPARE(appStatus->selectedNotes.get(), QList<int>{fixture.secondNoteId});
}

void NativeDesktopTests::rhiInlineTextEditingNavigatesCancelsAndUndoes_data() {
    QTest::addColumn<bool>("touch");
    QTest::newRow("mouse") << false;
    QTest::newRow("touch") << true;
}

void NativeDesktopTests::rhiInlineTextEditingNavigatesCancelsAndUndoes() {
    QFETCH(bool, touch);
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    ExistingRhiNoteFixture fixture;
    fixture.initialize(38400);
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
    auto *touchDevice = QTest::createTouchDevice();
    auto touchSequence = QTest::touchEvent(&canvas, touchDevice, false);
    const auto cancelPointer = qScopeGuard([&] {
        QEvent deactivate(QEvent::WindowDeactivate);
        QApplication::sendEvent(&canvas, &deactivate);
    });
    const auto beginEditing = [&](const QPoint &position, const QString &role) {
        QVERIFY(canvas.rect().contains(position));
        if (touch) {
            for (int tap = 0; tap < 2; ++tap) {
                touchSequence.press(0, position).commit();
                touchSequence.release(0, position).commit();
            }
        } else {
            QTest::mouseDClick(&canvas, Qt::LeftButton, Qt::NoModifier, position);
            QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, position);
        }
        auto *edit = canvas.findChild<QLineEdit *>();
        QVERIFY(edit);
        QTRY_VERIFY(edit->isVisible() && edit->hasFocus());
        QCOMPARE(edit->property("editRole").toString(), role);
        if (touch) {
            const auto version = fixture.runtime().documentVersion();
            const auto *undoBeforeTouch = historyManager->nextUndoEntry();
            const auto left = edit->mapTo(&canvas, QPoint(3, edit->height() / 2));
            const auto right = edit->mapTo(&canvas, QPoint(edit->width() - 4, edit->height() / 2));
            touchSequence.press(0, right).commit();
            touchSequence.move(0, left).commit();
            QVERIFY(edit->hasSelectedText());
            QTouchEvent cancel(QEvent::TouchCancel, touchDevice);
            cancel.setAccepted(false);
            QApplication::sendEvent(&canvas, &cancel);
            QVERIFY(cancel.isAccepted());
            touchSequence.release(0, left).commit();
            QVERIFY(edit->isVisible() && edit->hasFocus());
            QCOMPARE(fixture.runtime().documentVersion(), version);
            QCOMPARE(historyManager->nextUndoEntry(), undoBeforeTouch);
            QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
            touchSequence.press(0, right).commit();
            touchSequence.move(0, left).commit();
            QVERIFY(edit->hasSelectedText());
            touchSequence.release(0, left).commit();
            QVERIFY(edit->isVisible());
            QCOMPARE(fixture.runtime().documentVersion(), version);
            QCOMPARE(historyManager->nextUndoEntry(), undoBeforeTouch);
            QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
        }
    };
    beginEditing(fixture.pointFor(720, 60), QStringLiteral("Lyric"));
    if (QTest::currentTestFailed())
        return;
    auto *edit = canvas.findChild<QLineEdit *>();
    QCOMPARE(edit->text(), QStringLiteral("la"));
    QTest::keySequence(edit, QKeySequence::SelectAll);
    QTest::keyClicks(edit, "hello world");
    QTest::keyClick(edit, Qt::Key_Tab);
    QCOMPARE(first->lyric(), QStringLiteral("hello world"));
    QTRY_VERIFY(edit->isVisible() && edit->hasFocus());
    QCOMPARE(edit->text(), QStringLiteral("li"));
    QCOMPARE(appStatus->selectedNotes.get(), QList<int>{second->id()});
    QTest::keySequence(edit, QKeySequence::SelectAll);
    QTest::keyClicks(edit, "world");
    QTest::keyClick(edit, Qt::Key_Backtab);
    QCOMPARE(second->lyric(), QStringLiteral("world"));
    QTRY_VERIFY(edit->isVisible() && edit->hasFocus());
    QCOMPARE(edit->text(), QStringLiteral("hello world"));
    QCOMPARE(appStatus->selectedNotes.get(), QList<int>{first->id()});
    const auto beforeCancel = fixture.runtime().documentVersion();
    QTest::keySequence(edit, QKeySequence::SelectAll);
    QTest::keyClicks(edit, "discard this");
    QTest::keyClick(edit, Qt::Key_Escape);
    QTRY_VERIFY(!edit->isVisible());
    QCOMPARE(first->lyric(), QStringLiteral("hello world"));
    QCOMPARE(fixture.runtime().documentVersion(), beforeCancel);
    const auto contentBeforeHover = TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
    const auto *undoBeforeHover = historyManager->nextUndoEntry();
    QVERIFY(canvas.setViewScale(0.5, 2.0));
    QCOMPARE(canvas.scaleX(), 0.5);
    QVERIFY(canvas.centerAt(1920, 60));
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    auto *tooltip = canvas.findChild<ToolTip *>();
    QVERIFY(tooltip);
    QSignalSpy hoverCleared(&canvas, &PianoRollRhiWidget::keyHoverCleared);
    QSignalSpy keyHovered(&canvas, &PianoRollRhiWidget::keyHovered);
    const auto hoverAt = [&](const QPoint &position) {
        QCursor::setPos(canvas.mapToGlobal(position));
        QCoreApplication::processEvents();
        QMouseEvent move(QEvent::MouseMove, position, canvas.mapToGlobal(position), Qt::NoButton,
                         Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&canvas, &move);
    };
    hoverAt(fixture.pointFor(720, 60));
    QTRY_VERIFY2(tooltip->isVisible(), qPrintable(recentInput.join('\n')));
    QTextDocument tooltipText;
    tooltipText.setHtml(tooltip->title());
    QCOMPARE(tooltipText.toPlainText(), first->lyric());
    const auto clears = hoverCleared.size();
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(&canvas, &leave);
    QCOMPARE(hoverCleared.size(), clears + 1);
    QCOMPARE(canvas.cursor().shape(), Qt::ArrowCursor);
    QTRY_VERIFY(!tooltip->isVisible());
    const auto hovers = keyHovered.size();
    hoverAt(fixture.pointFor(720, 60));
    QTRY_VERIFY(tooltip->isVisible());
    QVERIFY(keyHovered.size() > hovers);
    QCOMPARE(keyHovered.last().first().toInt(), 60);
    hoverAt(QPoint(-20, canvas.height() / 2));
    QTRY_VERIFY(!tooltip->isVisible());
    QCOMPARE(fixture.runtime().documentVersion(), beforeCancel);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), contentBeforeHover);
    QCOMPARE(historyManager->nextUndoEntry(), undoBeforeHover);
    QVERIFY(canvas.setViewScale(1.0, 1.0));
    QVERIFY(canvas.centerAt(1920, 60));
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    historyManager->undo();
    QCOMPARE(second->lyric(), QStringLiteral("li"));
    historyManager->undo();
    QCOMPARE(first->lyric(), QStringLiteral("la"));
    QVERIFY(!historyManager->canUndo());

    {
        const auto beforeMode = fixture.runtime().documentVersion();
        const auto beforeModeContent =
            TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
        beginEditing(fixture.pointFor(720, 60), QStringLiteral("Lyric"));
        if (QTest::currentTestFailed())
            return;
        QTest::keySequence(edit, QKeySequence::SelectAll);
        QTest::keyClicks(edit, "committed on tool change");
        QCOMPARE(fixture.runtime().documentVersion(), beforeMode);
        canvas.setEditMode(ClipEditorGlobal::DrawNote);
        QTRY_VERIFY(!edit->isVisible());
        QCOMPARE(first->lyric(), QStringLiteral("committed on tool change"));
        QCOMPARE(second->lyric(), QStringLiteral("li"));
        QCOMPARE(fixture.runtime().documentVersion().revision, beforeMode.revision + 1);
        QVERIFY(!editSessionManager->hasActiveTransaction());
        fixture.waitForFrame();
        if (QTest::currentTestFailed())
            return;

        const auto committedText = TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
        const auto drawFrom = fixture.pointFor(2400, 64);
        const auto drawTo = fixture.pointFor(2880, 64);
        QVERIFY(canvas.rect().contains(drawFrom) && canvas.rect().contains(drawTo));
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, drawFrom);
        fixture.moveTo(drawTo);
        QCOMPARE(fixture.clip->notes().count(), 2);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, drawTo);
        QCOMPARE(fixture.clip->notes().count(), 3);
        QCOMPARE(fixture.runtime().documentVersion().revision, beforeMode.revision + 2);
        QVERIFY(!editSessionManager->hasActiveTransaction());
        QVERIFY(fixture.runtime().history().undo(fixture.command()));
        QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), committedText);
        QVERIFY(fixture.runtime().history().undo(fixture.command()));
        QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), beforeModeContent);
        QVERIFY(!historyManager->canUndo());
        canvas.setEditMode(ClipEditorGlobal::Select);
        fixture.waitForFrame();
        if (QTest::currentTestFailed())
            return;
    }

    {
        const auto defaults = appOptions->general()->defaultLyrics;
        const auto restoreDefaults =
            qScopeGuard([&] { appOptions->general()->defaultLyrics = defaults; });
        appOptions->general()->defaultLyrics[QStringLiteral("eng")] = QStringLiteral("ah");
        const auto beforeEmpty = fixture.runtime().documentVersion();
        const auto contentBeforeEmpty =
            TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
        beginEditing(fixture.pointFor(720, 60), QStringLiteral("Lyric"));
        if (QTest::currentTestFailed())
            return;
        QTest::keySequence(edit, QKeySequence::SelectAll);
        QTest::keyClick(edit, Qt::Key_Backspace);
        QVERIFY(edit->text().isEmpty());
        QTest::keyClick(edit, Qt::Key_Return);
        QTRY_VERIFY(!edit->isVisible());
        QCOMPARE(first->lyric(), QStringLiteral("ah"));
        QCOMPARE(second->lyric(), QStringLiteral("li"));
        QCOMPARE(fixture.runtime().documentVersion().revision, beforeEmpty.revision + 1);
        QVERIFY(!editSessionManager->hasActiveTransaction());
        historyManager->undo();
        QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel),
                 contentBeforeEmpty);
        QVERIFY(!historyManager->canUndo());
    }

    const auto contentBeforeRemoval =
        TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
    beginEditing(fixture.pointFor(720, 60), QStringLiteral("Lyric"));
    if (QTest::currentTestFailed())
        return;
    QTest::keySequence(edit, QKeySequence::SelectAll);
    QTest::keyClicks(edit, "discarded after deleting the note");
    const auto pendingText = edit->text();
    QVERIFY(fixture.runtime().notes().setPronunciation(
        fixture.command(), Automation::ClipId(fixture.clip->id()), Automation::NoteId(second->id()),
        true, QStringLiteral("lu")));
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    QVERIFY(edit->isVisible());
    QCOMPARE(edit->text(), pendingText);
    QCOMPARE(first->lyric(), QStringLiteral("la"));
    historyManager->undo();
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), contentBeforeRemoval);
    QVERIFY(edit->isVisible());
    QCOMPARE(edit->text(), pendingText);
    const auto beforeRemoval = fixture.runtime().documentVersion();
    QVERIFY(fixture.runtime().notes().removeNotes(fixture.command(),
                                                  Automation::ClipId(fixture.clip->id()),
                                                  {Automation::NoteId(first->id())}));
    const auto afterRemoval = fixture.runtime().documentVersion();
    QCOMPARE(afterRemoval.revision, beforeRemoval.revision + 1);
    const auto removedContent = TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
    QTRY_VERIFY(!edit->isVisible());
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(fixture.runtime().documentVersion(), afterRemoval);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), removedContent);
    historyManager->undo();
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), contentBeforeRemoval);
    QCOMPARE(fixture.clip->findNoteById(fixture.noteId), first);
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
    QTest::addColumn<bool>("penEraser");
    QTest::addColumn<bool>("finger");
    QTest::newRow("draw") << EditorViewGlobal::DrawPitch << false << false;
    QTest::newRow("trace-original") << EditorViewGlobal::TracePitch << false << false;
    QTest::newRow("erase") << EditorViewGlobal::ErasePitch << false << false;
    QTest::newRow("pen-eraser-under-draw-tool") << EditorViewGlobal::DrawPitch << true << false;
    QTest::newRow("finger-draw-and-system-cancel") << EditorViewGlobal::DrawPitch << false << true;
}

void NativeDesktopTests::rhiPitchStrokePreviewsCancelAndCommit() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    QFETCH(EditorViewGlobal::PianoRollEditMode, mode);
    QFETCH(bool, penEraser);
    QFETCH(bool, finger);
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
    const QPointingDevice pen(
        QStringLiteral("Fixture pitch eraser"), 1003, QInputDevice::DeviceType::Stylus,
        QPointingDevice::PointerType::Eraser,
        QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 2);
    auto *touchDevice = QTest::createTouchDevice();
    auto touchSequence = QTest::touchEvent(&canvas, touchDevice, false);
    const auto fingerEditing = appOptions->general()->drawParamWithFinger;
    if (finger)
        appOptions->general()->drawParamWithFinger = true;
    const auto cancelPointer = qScopeGuard([&] {
        if (finger) {
            QTouchEvent cancel(QEvent::TouchCancel, touchDevice);
            QApplication::sendEvent(&canvas, &cancel);
        }
        QEvent deactivate(QEvent::WindowDeactivate);
        QApplication::sendEvent(&canvas, &deactivate);
        appOptions->general()->drawParamWithFinger = fingerEditing;
    });
    const auto press = [&] {
        if (finger)
            touchSequence.press(0, start).commit();
        else if (penEraser)
            QVERIFY(TestSupport::sendTabletEvent(canvas, pen, QEvent::TabletPress, start, 0.7,
                                                 Qt::LeftButton, Qt::LeftButton));
        else
            QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, start);
    };
    const auto move = [&] {
        if (finger)
            touchSequence.move(0, finish).commit();
        else if (penEraser)
            QVERIFY(TestSupport::sendTabletEvent(canvas, pen, QEvent::TabletMove, finish, 0.7,
                                                 Qt::NoButton, Qt::LeftButton));
        else
            fixture.moveTo(finish);
    };
    const auto release = [&] {
        if (finger)
            touchSequence.release(0, finish).commit();
        else if (penEraser)
            QVERIFY(TestSupport::sendTabletEvent(canvas, pen, QEvent::TabletRelease, finish, 0,
                                                 Qt::LeftButton, Qt::NoButton));
        else
            QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, finish);
    };
    press();
    move();
    QVERIFY(editSessionManager->hasActiveTransaction());
    unchanged();
    QCOMPARE(fixture.runtime().documentVersion(), before);
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    if (finger) {
        QTouchEvent cancel(QEvent::TouchCancel, touchDevice);
        cancel.setAccepted(false);
        QApplication::sendEvent(&canvas, &cancel);
    } else {
        QTest::keyClick(&canvas, Qt::Key_Escape);
    }
    release();
    QVERIFY(!editSessionManager->hasActiveTransaction());
    unchanged();
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QVERIFY(!historyManager->canUndo());

    press();
    move();
    release();
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
    const auto checkDrawnSample = [&] {
        QVERIFY(sampleAt(720));
        // Integer mouse positions limit precision to one pixel of the pitch scale.
        const auto centsPerPixel = 100.0 / (ClipEditorGlobal::noteHeight * canvas.scaleY());
        QVERIFY(qAbs(*sampleAt(720) - 6200) <= centsPerPixel);
    };
    QCOMPARE(sampleAt(200), std::optional<int>(6100));
    QCOMPARE(sampleAt(1500), std::optional<int>(6100));
    if (mode == EditorViewGlobal::ErasePitch || penEraser) {
        QVERIFY(!sampleAt(720));
    } else if (mode == EditorViewGlobal::TracePitch) {
        QCOMPARE(sampleAt(720), std::optional<int>(6000));
    } else {
        checkDrawnSample();
    }
    const auto *currentOriginal =
        dynamic_cast<const DrawCurve *>(pitch->curves(Param::Original).first());
    QVERIFY(currentOriginal);
    QCOMPARE(*currentOriginal, originalBefore);
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    historyManager->undo();
    unchanged();
    QVERIFY(!historyManager->canUndo());
    if (penEraser) {
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, start);
        fixture.moveTo(finish);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, finish);
        checkDrawnSample();
        historyManager->undo();
        unchanged();
        QVERIFY(!historyManager->canUndo());
    }
    fixture.waitForFrame();
}

void NativeDesktopTests::rhiNoteSplittingSnapsAndUndoRestoresThePhrase_data() {
    QTest::addColumn<bool>("refusedPen");
    QTest::newRow("mouse") << false;
    QTest::newRow("refused-eraser-then-mouse") << true;
}

void NativeDesktopTests::rhiNoteSplittingSnapsAndUndoRestoresThePhrase() {
    QFETCH(bool, refusedPen);
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    if (refusedPen && QGuiApplication::platformName() == QStringLiteral("windows"))
        QSKIP("Windows native pen hover requires pointer messages from a physical device");
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
    QTest::mouseMove(canvas.windowHandle(), position);
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(fixture.clip->notes().count(), 1);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    if (refusedPen) {
        const auto content = TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
        const auto *undo = historyManager->nextUndoEntry();
        const QPointingDevice eraser(
            QStringLiteral("Fixture split-tool eraser"), 1012, QInputDevice::DeviceType::Stylus,
            QPointingDevice::PointerType::Eraser,
            QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 1);
        const auto leaveRange = [&] {
            QTabletEvent leave(QEvent::TabletLeaveProximity, &eraser, position,
                               canvas.mapToGlobal(position), 0.0, 0, 0, 0, 0, 0, Qt::NoModifier,
                               Qt::NoButton, Qt::NoButton);
            QApplication::sendEvent(qApp, &leave);
        };
        const auto restorePointer = qScopeGuard(leaveRange);
        TestSupport::sendTabletEvent(canvas, eraser, QEvent::TabletMove, position, 0.0,
                                     Qt::NoButton, Qt::NoButton);
        QMouseEvent hover(QEvent::MouseMove, position, canvas.mapToGlobal(position), Qt::NoButton,
                          Qt::NoButton, Qt::NoModifier, &eraser);
        QApplication::sendEvent(&canvas, &hover);
        QTRY_COMPARE(canvas.cursor().shape(), Qt::ForbiddenCursor);
        TestSupport::sendTabletEvent(canvas, eraser, QEvent::TabletPress, position, 0.8,
                                     Qt::LeftButton, Qt::LeftButton);
        TestSupport::sendTabletEvent(canvas, eraser, QEvent::TabletRelease, position, 0.0,
                                     Qt::LeftButton, Qt::NoButton);
        QCOMPARE(fixture.runtime().documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), content);
        QCOMPARE(historyManager->nextUndoEntry(), undo);
        QVERIFY(!editSessionManager->hasActiveTransaction());
        fixture.waitForFrame();
        if (QTest::currentTestFailed())
            return;
        leaveRange();
        // Qt filters QPA mouse moves that leave the global position unchanged.
        QTest::mouseMove(canvas.windowHandle(), position + QPoint(1, 0));
        QTRY_VERIFY(canvas.cursor().shape() != Qt::ForbiddenCursor);
        QTest::mouseMove(canvas.windowHandle(), position);
    }
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
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    historyManager->undo();
    QCOMPARE(fixture.clip->notes().count(), 1);
    const auto *restored = fixture.clip->findNoteById(fixture.noteId);
    QVERIFY(restored);
    QCOMPARE(restored->localStart(), 480);
    QCOMPARE(restored->length(), 480);
    QCOMPARE(restored->lyric(), QStringLiteral("la"));
    QVERIFY(!historyManager->canUndo());
    fixture.waitForFrame();
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
    canvas.setEditMode(ClipEditorGlobal::DrawNote);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, pronunciation);
    QCOMPARE(appStatus->selectedNotes.get(), QList<int>{fixture.noteId});
    QCOMPARE(fixture.clip->notes().count(), 2);
    QCOMPARE(fixture.runtime().documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
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
    fixture.initialize(38400);
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
        // QTest holds simulated buttons only; defer unrelated native input until release.
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(&canvas, end);
        const auto preview = appStatus->selectedNotes.get();
        if (preview.size() != 2) {
            qWarning() << "Unexpected marquee preview:" << preview << "press/release:" << start
                       << end << "native cursor:" << canvas.mapFromGlobal(QCursor::pos());
            qWarning().noquote() << recentInput.join('\n');
        }
        QCOMPARE(preview.size(), 2);
        fixture.waitForFrame(QEventLoop::ExcludeUserInputEvents);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, end);
        QCoreApplication::processEvents();
        const auto selected = appStatus->selectedNotes.get();
        if (selected != preview) {
            qWarning() << "Selection changed while presenting the drag preview:" << preview
                       << selected << "press/release:" << start << end
                       << "native cursor:" << canvas.mapFromGlobal(QCursor::pos());
            qWarning().noquote() << recentInput.join('\n');
        }
        QCOMPARE(selected, preview);
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
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, start);
        fixture.moveTo(end);
        QVERIFY(editSessionManager->hasActiveTransaction());
        QCOMPARE(first->localStart(), 480);
        QCOMPARE(second->localStart(), 1200);
        QCOMPARE(fixture.runtime().documentVersion(), before);
        fixture.waitForFrame();
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
    const auto *entry = historyManager->nextUndoEntry();
    QVERIFY(entry && entry->focusTransition());
    const auto focus = entry->focusTransition()->after;
    const auto editedVersion = fixture.runtime().documentVersion();
    const auto editedModel = TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
    const auto originalSize = canvas.size();
    const auto originalView = canvas.viewState();
    canvas.resize(originalSize.width(), 200);
    QVERIFY(canvas.setViewScale(5.0, 8.0));
    QVERIFY(canvas.centerAt(1440, 62));
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(canvas.focusVisibility(focus), HistoryFocusVisibility::ScrollRequired);
    const auto zoomed = canvas.viewState();
    QVERIFY(canvas.revealFocus(focus, false));
    QVERIFY(canvas.scaleX() < zoomed.horizontalScale);
    QVERIFY(canvas.scaleY() < zoomed.verticalScale);
    QCOMPARE(canvas.focusVisibility(focus), HistoryFocusVisibility::Visible);
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(fixture.runtime().documentVersion(), editedVersion);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), editedModel);
    QCOMPARE(historyManager->nextUndoEntry(), entry);
    canvas.resize(originalSize);
    QVERIFY(canvas.setViewScale(originalView.horizontalScale, originalView.verticalScale));
    QVERIFY(canvas.centerAt(originalView.centerTick, originalView.centerKeyIndex));
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
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

    const auto beforeCompact = fixture.runtime().documentVersion();
    const auto contentBeforeCompact =
        TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
    QVERIFY(canvas.setViewScale(0.2, 1.0));
    QCOMPARE(canvas.scaleX(), 0.2);
    QVERIFY(canvas.centerAt(1920, 60));
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    canvas.setEditMode(ClipEditorGlobal::Select);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, fixture.pointFor(3600, 65));
    QVERIFY(appStatus->selectedNotes.get().isEmpty());
    selectRange(64, 58);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(fixture.runtime().documentVersion(), beforeCompact);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), contentBeforeCompact);
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
    const auto previousCursor = QCursor::pos();
    const auto restore = qScopeGuard([&] { QCursor::setPos(previousCursor); });
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
    const auto runMenu = [&](QPoint position, const QString &text, bool paste, bool commit,
                             bool mixedInterpolation = false) {
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
            if (mixedInterpolation) {
                const auto items = menu->actions();
                for (const auto &label : {PianoRollContextMenuController::tr("Linear"),
                                          PianoRollContextMenuController::tr("Hermite")}) {
                    const auto entry =
                        std::find_if(items.cbegin(), items.cend(),
                                     [&](const auto *item) { return item->text() == label; });
                    QVERIFY(entry != items.cend());
                    QVERIFY((*entry)->isEnabled());
                    QVERIFY(!(*entry)->isChecked());
                }
            }
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
    const auto beforeAnchorMenu = TestSupport::projectSnapshot(*fixture.context->m_appModel);
    editor.onEditModeChanged(ClipEditorGlobal::EditPitchAnchor);
    runMenu(point(960, 61), PianoRollContextMenuController::tr("Linear"), false, true);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(anchorNodes().size(), 3);
    QCOMPARE(anchorNodes().at(0)->interpMode(), AnchorNode::Hermite);
    QCOMPARE(anchorNodes().at(1)->interpMode(), AnchorNode::Linear);
    QCOMPARE(anchorNodes().at(2)->interpMode(), AnchorNode::None);
    const auto beforeMixedMenu = TestSupport::projectSnapshot(*fixture.context->m_appModel);
    const auto mixedVersion = runtime.documentVersion();
    const auto *linearEntry = historyManager->nextUndoEntry();
    QTest::keyClick(canvas, Qt::Key_Escape);
    const auto selectStart = point(360, 64);
    const auto selectEnd = point(1200, 59);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, selectStart);
    QTest::mouseMove(canvas, selectEnd);
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, selectEnd);
    QCOMPARE(runtime.documentVersion(), mixedVersion);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.context->m_appModel), beforeMixedMenu);
    runMenu(point(960, 61), PianoRollContextMenuController::tr("Hermite"), false, true, true);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(anchorNodes().at(0)->interpMode(), AnchorNode::Hermite);
    QCOMPARE(anchorNodes().at(1)->interpMode(), AnchorNode::Hermite);
    QCOMPARE(anchorNodes().at(2)->interpMode(), AnchorNode::None);
    QCOMPARE(runtime.documentVersion().revision, mixedVersion.revision + 1);
    QVERIFY(runtime.history().undo(command()));
    QCOMPARE(TestSupport::projectSnapshot(*fixture.context->m_appModel), beforeMixedMenu);
    QCOMPARE(historyManager->nextUndoEntry(), linearEntry);
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
    QCOMPARE(TestSupport::projectSnapshot(*fixture.context->m_appModel), beforeAnchorMenu);
    QVERIFY(runtime.history().undo(command()));
    QVERIFY(pitch->curves(Param::Edited).isEmpty());
    QVERIFY(!historyManager->canUndo());

    editor.onEditModeChanged(ClipEditorGlobal::DrawNote);
    const auto beforeFallback = runtime.documentVersion();
    const auto sourceNote = clip->findNoteById(sourceId)->serialize();
    const auto viewport = editor.viewState();
    const auto onePixelTicks = (canvas->endTick() - canvas->startTick()) / canvas->width();
    QPointer<PianoRollRhiWidget> previousCanvas(canvas);
    QSignalSpy failures(canvas, &EditorRhiWidget::backendFailed);
    QTest::ignoreMessage(QtCriticalMsg, "[PianoRollRhi] QRhiWidget reported render failure");
    canvas->renderFailed();
    canvas->renderFailed();
    QCOMPARE(failures.size(), 0);
    QTRY_VERIFY(previousCanvas.isNull());
    QCOMPARE(failures.size(), 1);
    QCOMPARE(failures.first().first().toString(),
             QStringLiteral("QRhiWidget reported render failure"));
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

void NativeDesktopTests::rhiInferenceErrorBadgesExplainOverlapsAndFollowUndo() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    ExistingRhiNoteFixture fixture;
    fixture.initialize();
    if (QTest::currentTestFailed())
        return;
    fixture.configureInference();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = fixture.runtime();
    const auto clipId = Automation::ClipId(fixture.clip->id());
    QList<Automation::NoteDraftDto> additional;
    for (const auto tick : {960, 1440}) {
        Automation::NoteDraftDto note;
        note.localStart = tick;
        note.length = 480;
        note.keyIndex = 60;
        note.lyric = TestSupport::fixtureLyric();
        note.language = TestSupport::fixtureLanguage();
        additional.append(note);
    }
    QVERIFY(runtime.notes().insertNotes(fixture.command(), clipId, additional));
    QTRY_VERIFY_WITH_TIMEOUT(fixture.inferenceSettled(), 15000);
    QCOMPARE(fixture.clip->notes().count(), 3);
    QVERIFY(fixture.clip->noteInferenceErrors().isEmpty());
    const auto notes = fixture.clip->notes().toList();
    const auto *first = notes.at(0);
    const auto *second = notes.at(1);
    const auto *third = notes.at(2);
    historyManager->reset();
    const auto cleanProject = TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
    const auto initialVersion = runtime.documentVersion();
    QVERIFY(runtime.notes().moveNotes(fixture.command(), clipId, {Automation::NoteId(second->id())},
                                      -240, 0));
    QCOMPARE(runtime.documentVersion().revision, initialVersion.revision + 1);
    const auto *editEntry = historyManager->nextUndoEntry();
    QVERIFY(editEntry);
    QTRY_VERIFY_WITH_TIMEOUT(fixture.clip->noteInferenceErrors().contains(first->id()) &&
                                 fixture.clip->noteInferenceErrors().contains(second->id()) &&
                                 fixture.inferenceSettled(),
                             15000);
    QCOMPARE(fixture.clip->noteInferenceErrors().value(first->id()).reason,
             SliceExclusionReason::Overlapped);
    QVERIFY(!fixture.clip->noteInferenceErrors().contains(third->id()));
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    auto &canvas = *fixture.canvas;
    const auto badgeCenter = [&](const Note *note) {
        const auto topLeft = fixture.pointFor(note->localStart(), note->keyIndex() + 0.5);
        const auto topRight =
            fixture.pointFor(note->localStart() + note->length(), note->keyIndex() + 0.5);
        const QRectF rect(topLeft, QSizeF(topRight.x() - topLeft.x(),
                                          ClipEditorGlobal::noteHeight * canvas.scaleY()));
        return PianoRollGraphicsViewHelper::noteErrorBadgeRect(rect).center().toPoint();
    };
    const auto errorToolTip = [&]() -> ToolTip * {
        for (auto *tip : canvas.findChildren<ToolTip *>()) {
            QTextDocument text;
            text.setHtml(tip->title());
            if (tip->isVisible() && text.toPlainText() == QStringLiteral("Overlapping note"))
                return tip;
        }
        return nullptr;
    };
    const auto overlappingVersion = runtime.documentVersion();
    const auto overlappingProject = TestSupport::projectSnapshot(*fixture.app.context->m_appModel);
    const auto selected = appStatus->selectedNotes.get();
    fixture.hoverAt(fixture.pointFor(1800, 66));
    fixture.hoverAt(badgeCenter(first));
    QTRY_VERIFY2(errorToolTip(), qPrintable(recentInput.join('\n')));
    QTextDocument message;
    message.setHtml(errorToolTip()->message().join('\n'));
    QCOMPARE(message.toPlainText(),
             QStringLiteral("This note overlaps another note and is ignored"));
    fixture.hoverAt(fixture.pointFor(1800, 66));
    QTRY_VERIFY(!errorToolTip());
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, badgeCenter(second));
    QTRY_VERIFY(errorToolTip());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(appStatus->selectedNotes.get(), selected);
    QCOMPARE(runtime.documentVersion(), overlappingVersion);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), overlappingProject);
    QCOMPARE(historyManager->nextUndoEntry(), editEntry);
    QVERIFY(runtime.history().undo(fixture.command()));
    QTRY_VERIFY_WITH_TIMEOUT(
        fixture.clip->noteInferenceErrors().isEmpty() && fixture.inferenceSettled(), 15000);
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    QTRY_VERIFY(!errorToolTip());
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), cleanProject);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(runtime.history().redo(fixture.command()));
    QTRY_VERIFY_WITH_TIMEOUT(fixture.clip->noteInferenceErrors().contains(first->id()) &&
                                 fixture.inferenceSettled(),
                             15000);
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(TestSupport::projectSnapshot(*fixture.app.context->m_appModel), overlappingProject);
    fixture.hoverAt(fixture.pointFor(1800, 66));
    fixture.hoverAt(badgeCenter(first));
    QTRY_VERIFY(errorToolTip());
}

void NativeDesktopTests::rhiPitchModulationUsesTheInferredBaseline() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("RHI widgets require a native window backend");
    ExistingRhiNoteFixture fixture;
    fixture.initialize();
    if (QTest::currentTestFailed())
        return;
    fixture.configureInference();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = fixture.runtime();
    const auto clipId = Automation::ClipId(fixture.clip->id());
    const auto settled = [&] { return fixture.inferenceSettled(); };
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
    auto before = runtime.documentVersion();
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
        for (const auto &boundary :
             {qMakePair(fixture.pointFor(545, 60), start),
              qMakePair(start, fixture.pointFor(545, 60)),
              qMakePair(fixture.pointFor(895, 60), end), qMakePair(end, fixture.pointFor(895, 60)),
              qMakePair(start, fixture.pointFor(520, 60)),
              qMakePair(end, fixture.pointFor(920, 60))}) {
            QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, boundary.first);
            fixture.moveTo(boundary.second);
            QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, boundary.second);
            fixture.waitForFrame();
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
    const auto outside = fixture.pointFor(1600, 60);
    QVERIFY(canvas.rect().contains(outside));
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, outside);
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, press);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, press);
    QCOMPARE(snapshot(), initial);
    QCOMPARE(runtime.documentVersion(), before);
    const auto originalName = fixture.clip->name();
    const auto *targetNote = fixture.clip->findNoteById(fixture.noteId);
    QVERIFY(targetNote);
    const auto originalStart = targetNote->localStart();
    const auto timeline = runtime.timeline().getTimeline(runtime.documentVersion().documentId);
    QVERIFY(timeline && !timeline.get().tempos.isEmpty());
    const auto originalTempos = timeline.get().tempos;
    for (const auto change : {QByteArrayLiteral("clip-name"), QByteArrayLiteral("note-position"),
                              QByteArrayLiteral("tempo")}) {
        selectRange();
        if (QTest::currentTestFailed())
            return;
        if (change == QByteArrayLiteral("note-position")) {
            QVERIFY(runtime.notes().moveNotes(fixture.command(), clipId,
                                              {Automation::NoteId(fixture.noteId)}, 120, 0));
            QCOMPARE(targetNote->localStart(), originalStart + 120);
        } else if (change == QByteArrayLiteral("clip-name")) {
            QVERIFY(runtime.project().renameClip(fixture.command(), clipId,
                                                 QStringLiteral("Renamed while selecting pitch")));
            QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
        } else {
            const auto tempo = originalTempos.first().value + 30.0;
            QVERIFY(runtime.timeline().setTempo(fixture.command(), 0, tempo));
            const auto changedTimeline =
                runtime.timeline().getTimeline(runtime.documentVersion().documentId);
            QVERIFY(changedTimeline);
            QCOMPARE(changedTimeline.get().tempos.first().value, tempo);
        }
        QTRY_VERIFY_WITH_TIMEOUT(settled(), 15000);
        const auto changed = runtime.documentVersion();
        QVERIFY(changed.revision > before.revision);
        QCOMPARE(snapshot(), initial);
        fixture.waitForFrame();
        if (QTest::currentTestFailed())
            return;
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, press);
        QVERIFY(!editSessionManager->hasActiveTransaction());
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, press);
        QCOMPARE(runtime.documentVersion(), changed);
        QCOMPARE(snapshot(), initial);
        QVERIFY(runtime.history().undo(fixture.command()));
        QTRY_VERIFY_WITH_TIMEOUT(settled(), 15000);
        QCOMPARE(fixture.clip->name(), originalName);
        QCOMPARE(targetNote->localStart(), originalStart);
        const auto restoredTimeline =
            runtime.timeline().getTimeline(runtime.documentVersion().documentId);
        QVERIFY(restoredTimeline);
        QCOMPARE(restoredTimeline.get().tempos, originalTempos);
        QCOMPARE(snapshot(), initial);
        before = runtime.documentVersion();
        QVERIFY(!historyManager->canUndo());
        fixture.waitForFrame();
        if (QTest::currentTestFailed())
            return;
    }
    for (const bool rightButton : {false, true}) {
        selectRange();
        if (QTest::currentTestFailed())
            return;
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, press);
        fixture.moveTo(release);
        QVERIFY(editSessionManager->hasActiveTransaction());
        QCOMPARE(snapshot(), initial);
        QCOMPARE(runtime.documentVersion(), before);
        if (rightButton)
            QTest::mouseClick(&canvas, Qt::RightButton, Qt::NoModifier, release);
        else
            QTest::keyClick(&canvas, Qt::Key_Escape);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, release);
        QVERIFY(!editSessionManager->hasActiveTransaction());
        QCOMPARE(snapshot(), initial);
        QCOMPARE(runtime.documentVersion(), before);
        QVERIFY(!historyManager->canUndo());
        fixture.waitForFrame();
        if (QTest::currentTestFailed())
            return;
    }

    selectRange();
    if (QTest::currentTestFailed())
        return;
    const auto factorHandle = QPoint(press.x(), 20);
    const auto factorRelease = factorHandle + QPoint(0, 100);
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, factorHandle);
    fixture.moveTo(factorRelease);
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
    fixture.waitForFrame();
    if (QTest::currentTestFailed())
        return;
    historyManager->undo();
    QCOMPARE(snapshot(), initial);
    QVERIFY(!historyManager->canUndo());
    QTRY_VERIFY_WITH_TIMEOUT(settled(), 15000);
    QCOMPARE(snapshot(), initial);
}
