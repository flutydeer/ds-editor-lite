#include "AppContext.h"

#include "Automation/CoreRuntime.h"
#include "Automation/AppOptionsAutomationAdapter.h"
#include "Automation/AudioExportAutomationAdapter.h"
#include "Automation/FileAutomationAdapter.h"
#include "Automation/ExtractionAutomationAdapter.h"
#include "Automation/InferenceAutomationAdapter.h"
#include "Automation/PackageAutomationAdapter.h"

#include <lite/Core/SingletonRegistry.h>
#include <lite/BuildInfo.h>

// Business singletons — all headers included here
#include <lite/ProjectModel/AppModel/AppModel.h>
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Model/Utils/ParamUtils.h"
#include <lite/Tasking/TaskManager.h>
#include <lite/History/HistoryManager.h>
#include <lite/PackageManager/PackageManager.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/InferController.h"
#include "Modules/Inference/EditSessionManager.h"
#include "Modules/Extractors/PitchExtractController.h"
#include "Modules/Extractors/MidiExtractController.h"
#include "Modules/ProjectConverters/DspxProjectConverterUi.h"
#include "Controller/AppController.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/AudioDecodingController.h"
#include "Controller/ClipboardController.h"
#include "Controller/TrackController.h"
#include "Controller/ClipController.h"
#include "Controller/EditorViewController.h"
#include "Controller/UndoRedoController.h"
#include "Controller/PlaybackController.h"
#include "Controller/ProjectStatusController.h"
#include "Controller/ProjectPackageResolver.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/Audio/subsystem/MidiSystem.h"
#include "Modules/Audio/subsystem/OutputSystem.h"
#include "Modules/Audio/utils/DeviceTester.h"
#include <lite/GUI/Controls/Toast.h>
#include "UI/Controls/LevelMeterManager.h"
#include "Global/AppGlobal.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QObject>
#include <QSysInfo>
#include <QThread>
#include <QTimer>

#if defined(WITH_DIRECT_MANIPULATION)
#  include <QWDMHCore/DirectManipulationSystem.h>
#endif

// AudioSystemContext — moved verbatim from old main.cpp
struct AudioSystemContext {
    AudioSystemContext() {
        AudioSystem::outputSystem()->initialize();
        AudioSystem::midiSystem()->initialize();
        // Managed by Qt Object System. No need for manual memory management.
        new DeviceTester(&audioSystem);
        new AudioContext(&audioSystem);
    }

    AudioSystem audioSystem;
};

#if defined(WITH_DIRECT_MANIPULATION)
struct DirectManipulationHolder {
    QWDMH::DirectManipulationSystem system;
};
#endif

struct AppContext::GuiContext {
    LevelMeterManager *levelMeterInstance = nullptr;
    ClipboardController *clipboardInstance = nullptr;
    TrackController *trackInstance = nullptr;
    ClipController *clipInstance = nullptr;
    EditorViewController *editorViewInstance = nullptr;
    UndoRedoController *undoRedoInstance = nullptr;
    PitchExtractController *pitchExtractInstance = nullptr;
    MidiExtractController *midiExtractInstance = nullptr;
    ProjectStatusController *projectStatusInstance = nullptr;
    AppController *appInstance = nullptr;
    DocumentWorkflowController *documentWorkflowInstance = nullptr;
#if defined(WITH_DIRECT_MANIPULATION)
    std::unique_ptr<DirectManipulationHolder> directManip;
#endif
};

AppContext *AppContext::s_self = nullptr;

AppContext::AppContext(std::unique_ptr<AppOptions> options, const AppHostMode hostMode)
    : m_hostMode(hostMode) {
    s_self = this;

    // SingletonRegistry::create constructs each service (bypassing its private ctor via
    // the SingletonRegistry friendship) and registers it immediately, so that a later
    // service's constructor — which may call Xxx::instance() — resolves the
    // owned instance instead of falling back to a stray Meyers static. This
    // object still owns the returned pointers and tears them down, in reverse,
    // via SingletonRegistry::destroy in the destructor.

    // L0: Basic data models (no dependencies)
    m_appStatus = SingletonRegistry::create<AppStatus>();
    // AppOptions is constructed by main() and handed in; just register it.
    m_appOptions = options.release();
    SingletonRegistry::add(m_appOptions);
    m_appModel = SingletonRegistry::create<AppModel>();
    m_paramUtils = SingletonRegistry::create<ParamUtils>();

    // L1: Independent modules
    TaskManager::instance(); // force early construction on the main thread
    m_historyManager = SingletonRegistry::create<HistoryManager>();
    m_packageManager = SingletonRegistry::create<PackageManager>();
    Automation::DocumentRuntimeServices documentServices;
    documentServices.applyLoopSettings = [status = m_appStatus](const LoopSettings &settings) {
        status->loopSettings.set(settings);
    };
    documentServices.saveProject = [this](const QString &path, AppModel *model, QString &error) {
        DspxProjectConverterUi converter(m_appStatus->loopSettings);
        return converter.save(path, model, error);
    };
    Automation::PlaybackRuntimeServices playbackServices;
    playbackServices.snapshot = [this] {
        Automation::PlaybackHostSnapshot result;
        if (!m_playbackController)
            return result;
        switch (m_playbackController->playbackStatus()) {
            case PlaybackGlobal::Playing:
                result.state = Automation::PlaybackState::Playing;
                break;
            case PlaybackGlobal::Paused:
                result.state = Automation::PlaybackState::Paused;
                break;
            case PlaybackGlobal::Stopped:
                result.state = Automation::PlaybackState::Stopped;
                break;
        }
        result.position = m_playbackController->position();
        result.lastPosition = m_playbackController->lastPosition();
        result.loop = m_appStatus->loopSettings;
        return result;
    };
    playbackServices.canStart = [this] {
        return m_appStatus->currentEditObject.get() == AppStatus::EditObjectType::None;
    };
    playbackServices.play = [this] {
        return m_playbackController && m_playbackController->applyPlay();
    };
    playbackServices.pause = [this] {
        if (m_playbackController)
            m_playbackController->applyPause();
    };
    playbackServices.stop = [this] {
        if (m_playbackController)
            m_playbackController->applyStop();
    };
    playbackServices.setPosition = [this](const double tick) {
        if (m_playbackController)
            m_playbackController->applyPosition(tick);
    };
    playbackServices.setLastPosition = [this](const double tick) {
        if (m_playbackController)
            m_playbackController->applyLastPosition(tick);
    };
    playbackServices.setLoop = [status = m_appStatus](const LoopSettings &settings) {
        status->loopSettings.set(settings);
    };
    Automation::EditorRuntimeServices editorServices;
    editorServices.captureView = [this] {
        auto *controller = guiEditorViewController();
        return controller ? controller->captureState() : std::optional<EditorViewState>();
    };
    editorServices.captureStableState = [this] {
        const auto selectedClips = m_appStatus->selectedClips.get();
        const auto selectedNotes = m_appStatus->selectedNotes.get();
        return Automation::EditorStableState{
            .selectedTrackIndex = m_appStatus->selectedTrackIndex,
            .activeClipId = m_appStatus->activeClipId,
            .selectedClipIds = selectedClips,
            .selectedNoteIds = selectedNotes,
            .primaryClipId = m_appStatus->primarySelectedClipId,
            .primaryNoteId = m_appStatus->primarySelectedNoteId,
            .pianoRollQuantize = m_appStatus->pianoRollQuantize,
            .pianoRollQuantizeEnabled = m_appStatus->pianoRollQuantizeEnabled,
            .trackAutoPageTurnEnabled = m_appStatus->trackAutoPageTurnEnabled,
            .pianoRollAutoPageTurnEnabled = m_appStatus->pianoRollAutoPageTurnEnabled,
            .parameterEditInProgress =
                m_appStatus->currentEditObject.get() == AppStatus::EditObjectType::Param,
        };
    };
    editorServices.restoreView = [this](const EditorViewState &state) {
        auto *controller = guiEditorViewController();
        return controller && controller->applyRestoreState(state);
    };
    editorServices.centerTrackPanel = [this](const double tick, const double trackIndex) {
        auto *controller = guiEditorViewController();
        return controller && controller->applyCenterTrackPanelAt(tick, trackIndex);
    };
    editorServices.setTrackPanelScale = [this](const double horizontal, const double vertical) {
        auto *controller = guiEditorViewController();
        return controller && controller->applyTrackPanelScale(horizontal, vertical);
    };
    editorServices.setTrackPanelViewport = [this](const TrackPanelViewState &state) {
        auto *controller = guiEditorViewController();
        return controller && controller->applyTrackPanelViewport(state);
    };
    editorServices.setPanelVisibility = [this](const bool trackVisible, const bool bottomVisible) {
        auto *controller = guiEditorViewController();
        return controller && controller->applyPanelVisibility(trackVisible, bottomVisible);
    };
    editorServices.showBottomPanelPage = [this](const QString &pageId) {
        auto *controller = guiEditorViewController();
        return controller && controller->applyBottomPanelPage(pageId);
    };
    editorServices.showRegion = [this](const EditorViewGlobal::Region region) {
        auto *controller = guiEditorViewController();
        return controller && controller->applyShowRegion(region);
    };
    editorServices.focusRegion = [this](const EditorViewGlobal::Region region) {
        auto *controller = guiEditorViewController();
        return controller && controller->applyFocusRegion(region);
    };
    editorServices.centerPianoRoll = [this](const double tick, const double keyIndex) {
        auto *controller = guiEditorViewController();
        return controller && controller->applyCenterPianoRollAt(tick, keyIndex);
    };
    editorServices.setPianoRollScale = [this](const double horizontal, const double vertical) {
        auto *controller = guiEditorViewController();
        return controller && controller->applyPianoRollScale(horizontal, vertical);
    };
    editorServices.setClipEditorTimeViewport = [this](const double centerTick,
                                                      const double horizontalScale) {
        auto *controller = guiEditorViewController();
        return controller && controller->applyClipEditorTimeViewport(centerTick, horizontalScale);
    };
    editorServices.setPianoRollPitchViewport = [this](const double centerKeyIndex,
                                                      const double verticalScale) {
        auto *controller = guiEditorViewController();
        return controller && controller->applyPianoRollPitchViewport(centerKeyIndex, verticalScale);
    };
    editorServices.setPianoRollEditMode = [this](const EditorViewGlobal::PianoRollEditMode mode) {
        auto *controller = guiEditorViewController();
        return controller && controller->applyPianoRollEditMode(mode);
    };
    editorServices.setParameterForeground = [this](const ParamInfo::Name name) {
        auto *controller = guiEditorViewController();
        return controller && controller->applyParameterForeground(name);
    };
    editorServices.setParameterBackground = [this](const ParamInfo::Name name) {
        auto *controller = guiEditorViewController();
        return controller && controller->applyParameterBackground(name);
    };
    editorServices.swapParameters = [this] {
        auto *controller = guiEditorViewController();
        return controller && controller->applySwapParameters();
    };
    editorServices.setParameterEditMode = [this](const EditorViewGlobal::ParameterEditMode mode) {
        auto *controller = guiEditorViewController();
        return controller && controller->applyParameterEditMode(mode);
    };
    editorServices.setParameterValueViewport = [this](const double centerRatio,
                                                      const double verticalScale) {
        auto *controller = guiEditorViewController();
        return controller && controller->applyParameterValueViewport(centerRatio, verticalScale);
    };
    editorServices.setActiveClip = [this](const int clipId) {
        if (m_appStatus->activeClipId == clipId)
            return;
        m_appStatus->selectedNotes = QList<int>();
        m_appStatus->primarySelectedNoteId = -1;
        m_appStatus->activeClipId = clipId;
    };
    editorServices.setSelectedTrackIndex = [this](const int trackIndex) {
        m_appStatus->selectedTrackIndex = trackIndex;
    };
    editorServices.setSelectedClips = [this](const QList<int> &clipIds, const int primaryClipId) {
        m_appStatus->selectedClips = clipIds;
        m_appStatus->primarySelectedClipId = primaryClipId;
    };
    editorServices.setSelectedNotes = [this](const int clipId, const QList<int> &noteIds,
                                             const int primaryNoteId) {
        if (m_appStatus->activeClipId != clipId) {
            m_appStatus->selectedNotes = QList<int>();
            m_appStatus->primarySelectedNoteId = -1;
            m_appStatus->activeClipId = clipId;
        }
        m_appStatus->selectedNotes = noteIds;
        m_appStatus->primarySelectedNoteId = primaryNoteId;
    };
    editorServices.setPianoRollQuantize = [this](const int quantize, const bool enabled) {
        m_appStatus->pianoRollQuantize = quantize;
        m_appStatus->pianoRollQuantizeEnabled = enabled;
    };
    editorServices.setAutoPageTurn = [this](const Automation::EditorAutoPageTarget target,
                                            const bool enabled) {
        if (target == Automation::EditorAutoPageTarget::TrackPanel)
            m_appStatus->trackAutoPageTurnEnabled = enabled;
        else
            m_appStatus->pianoRollAutoPageTurnEnabled = enabled;
    };
    editorServices.focusVisibility = [this](const HistoryFocus &focus) {
        auto *controller = guiEditorViewController();
        return controller ? controller->focusVisibility(focus)
                          : HistoryFocusVisibility::Unavailable;
    };
    editorServices.revealFocus = [this](const HistoryFocus &focus, const bool finalize) {
        auto *controller = guiEditorViewController();
        return controller && controller->applyRevealFocus(focus, finalize);
    };
    Automation::ApplicationRuntimeServices applicationServices;
    applicationServices.info = [] {
        return Automation::ApplicationInfoDto{
            .name = QCoreApplication::applicationName(),
            .version = QCoreApplication::applicationVersion(),
            .platform = QSysInfo::prettyProductName(),
            .buildId = QString::fromLatin1(LITE_GIT_LAST_COMMIT_HASH),
        };
    };
    applicationServices.requestTermination =
        [this](const Automation::ApplicationTerminationMode mode,
               const Automation::ApplicationTerminationSavePolicy savePolicy) {
            auto *workflow = guiDocumentWorkflowController();
            if (workflow) {
                const auto workflowMode = mode == Automation::ApplicationTerminationMode::Exit
                                              ? TerminationMode::Exit
                                              : TerminationMode::Restart;
                const auto workflowPolicy =
                    savePolicy == Automation::ApplicationTerminationSavePolicy::Prompt
                        ? TerminationSavePolicy::Prompt
                    : savePolicy == Automation::ApplicationTerminationSavePolicy::Discard
                        ? TerminationSavePolicy::Discard
                        : TerminationSavePolicy::RejectUnsaved;
                switch (workflow->requestTermination(workflowMode, workflowPolicy)) {
                    case TerminationRequestResult::Accepted:
                        return Automation::ApplicationTerminationRequestResult::Accepted;
                    case TerminationRequestResult::Busy:
                        return Automation::ApplicationTerminationRequestResult::Busy;
                    case TerminationRequestResult::UnsavedChanges:
                        return Automation::ApplicationTerminationRequestResult::UnsavedChanges;
                }
                return Automation::ApplicationTerminationRequestResult::Unavailable;
            }

            if (!m_coreRuntime || !QCoreApplication::instance())
                return Automation::ApplicationTerminationRequestResult::Unavailable;
            const auto document = m_coreRuntime->documentVersion();
            if (m_coreRuntime->documentBusy(document.documentId))
                return Automation::ApplicationTerminationRequestResult::Busy;
            if (savePolicy != Automation::ApplicationTerminationSavePolicy::Discard &&
                !m_historyManager->isOnSavePoint()) {
                return Automation::ApplicationTerminationRequestResult::UnsavedChanges;
            }

            auto *application = QCoreApplication::instance();
            const auto restart = mode == Automation::ApplicationTerminationMode::Restart;
            QTimer::singleShot(0, application, [application, restart] {
                if (restart)
                    application->setProperty("restart", true);
                application->quit();
            });
            return Automation::ApplicationTerminationRequestResult::Accepted;
        };
    m_coreRuntime = std::make_unique<Automation::CoreRuntime>(
        m_appModel, m_historyManager, std::move(documentServices), std::move(playbackServices),
        std::move(editorServices), Automation::createAppOptionsAutomationServices(m_appOptions),
        Automation::createAppOptionsPresetAutomationServices(m_appOptions),
        Automation::createPackageAutomationServices(m_packageManager, m_appOptions),
        Automation::createInferenceAutomationServices(), Automation::createFileAutomationServices(),
        Automation::createAudioExportAutomationServices(),
        Automation::createExtractionAutomationServices(m_appOptions, TaskManager::instance()),
        std::move(applicationServices),
        m_hostMode == AppHostMode::Gui
            ? std::optional<Automation::WindowId>(Automation::WindowId::create())
            : std::nullopt);

    // L3: Runtime host must outlive the inference facade.
    m_synthrtEngine = SingletonRegistry::create<SynthrtEngine>();
    m_inferEngine = SingletonRegistry::create<InferEngine>();
    m_inferEngine->startInitialization();

    // L4: Core controllers (no construction-time cross-deps)
    m_audioDecodingController = SingletonRegistry::create<AudioDecodingController>();
    m_editSessionManager = SingletonRegistry::create<EditSessionManager>();

    // L5: Core controllers with construction-time deps
    m_playbackController = SingletonRegistry::create<PlaybackController>();
    m_playbackController->setLoopPreviewHandler(
        [status = m_appStatus](const LoopSettings &settings) {
            status->loopSettings.set(settings);
        });
    // ProjectPackageResolver connects to AppModel + PackageManager + AppStatus
    m_projectPackageResolver = SingletonRegistry::create<ProjectPackageResolver>();

    // L6: InferController connects to AppOptions, AppStatus, EditSessionManager, PlaybackController
    m_inferController = SingletonRegistry::create<InferController>();

    // Audio system (replaces old AudioSystemContext)
    m_audio = std::make_unique<AudioSystemContext>();

    initializeCommonWiring();

    if (m_hostMode == AppHostMode::Gui) {
        m_guiContext = std::make_unique<GuiContext>();

        // GUI composition is deliberately created only after every core service is available.
        m_guiContext->levelMeterInstance = SingletonRegistry::create<LevelMeterManager>(m_appModel);
        m_guiContext->clipboardInstance = SingletonRegistry::create<ClipboardController>();
        m_guiContext->trackInstance = SingletonRegistry::create<TrackController>();
        m_guiContext->clipInstance = SingletonRegistry::create<ClipController>();
        m_guiContext->editorViewInstance = SingletonRegistry::create<EditorViewController>();
        m_guiContext->undoRedoInstance = SingletonRegistry::create<UndoRedoController>();
        m_guiContext->pitchExtractInstance = SingletonRegistry::create<PitchExtractController>();
        m_guiContext->midiExtractInstance = SingletonRegistry::create<MidiExtractController>();
        m_guiContext->projectStatusInstance = SingletonRegistry::create<ProjectStatusController>();
#if defined(WITH_DIRECT_MANIPULATION)
        m_guiContext->directManip = std::make_unique<DirectManipulationHolder>();
#endif
        m_guiContext->appInstance = SingletonRegistry::create<AppController>();
        m_guiContext->documentWorkflowInstance =
            SingletonRegistry::create<DocumentWorkflowController>();
        m_audioDecodingController->setGuiServices(
            m_guiContext->documentWorkflowInstance,
            [](const QString &message) { Toast::show(message); });
    }
}

AppHostMode AppContext::hostMode() const {
    return m_hostMode;
}

bool AppContext::hasGui() const {
    return m_guiContext != nullptr;
}

bool AppContext::initializeDefaultDocument(QString *error) {
    const auto draft = Automation::DocumentAutomationFacade::newDocumentDraft(true);
    const auto result = m_coreRuntime->documents().commitNewDocument(
        {.expected = m_coreRuntime->documentVersion(),
         .source = Automation::InvocationSource::InternalAutomation},
        draft);
    if (result)
        return true;
    if (error)
        *error = result.getError().message;
    return false;
}

void AppContext::initializeCommonWiring() {
    // Push app-provided new-track defaults into the model for both GUI and QCore hosts.
    m_appModel->setPaletteColorCount(AppGlobal::paletteColorCount);
    const auto pushModelDefaults = [model = m_appModel, options = m_appOptions] {
        model->setDefaultSingingLanguage(options->general()->defaultSingingLanguage);
    };
    pushModelDefaults();
    QObject::connect(m_appOptions, &AppOptions::optionsChanged, m_appModel,
                     [pushModelDefaults](const AppOptionsGlobal::Option option) {
                         if (option == AppOptionsGlobal::All || option == AppOptionsGlobal::General)
                             pushModelDefaults();
                     });

    // Map the package library lifecycle onto AppStatus without involving GUI composition.
    QObject::connect(m_packageManager, &PackageManager::moduleStatusChanged, m_appStatus,
                     [status = m_appStatus](const PackageManager::ModuleStatus moduleStatus) {
                         status->packageModuleStatus = [moduleStatus] {
                             switch (moduleStatus) {
                                 case PackageManager::ModuleStatus::Loading:
                                     return AppStatus::ModuleStatus::Loading;
                                 case PackageManager::ModuleStatus::Ready:
                                     return AppStatus::ModuleStatus::Ready;
                                 case PackageManager::ModuleStatus::Error:
                                     return AppStatus::ModuleStatus::Error;
                             }
                             return AppStatus::ModuleStatus::Unknown;
                         }();
                     });

    QObject::connect(m_appModel, &AppModel::modelChanged, m_audioDecodingController,
                     &AudioDecodingController::onModelChanged);
    QObject::connect(m_appModel, &AppModel::trackChanged, m_audioDecodingController,
                     &AudioDecodingController::onTrackChanged);
}

EditorViewController *AppContext::guiEditorViewController() const {
    return m_guiContext ? m_guiContext->editorViewInstance : nullptr;
}

DocumentWorkflowController *AppContext::guiDocumentWorkflowController() const {
    return m_guiContext ? m_guiContext->documentWorkflowInstance : nullptr;
}

AppContext::~AppContext() {
    // Reverse order of construction. GUI services are absent in the QCore host.
    if (m_guiContext) {
        m_audioDecodingController->setGuiServices(nullptr);
        SingletonRegistry::destroy(m_guiContext->documentWorkflowInstance);

        // GUI controllers outlive MainWindow teardown and are released before core services.
        SingletonRegistry::destroy(m_guiContext->appInstance);
    }

    // Runtime users must finish before controllers and the runtime host are destroyed.
    const auto appThread = QCoreApplication::instance()->thread();
    const auto drainTasks = [appThread] {
        auto *tm = TaskManager::instance();
        tm->terminateAllTasks();
        tm->wait();
        if (tm->thread() != appThread) {
            tm->moveToThread(appThread);
        }
    };
    const auto taskThread = TaskManager::instance()->thread();
    if (taskThread == QThread::currentThread() || !taskThread->isRunning()) {
        drainTasks();
    } else {
        QMetaObject::invokeMethod(TaskManager::instance(), drainTasks,
                                  Qt::BlockingQueuedConnection);
    }

    // Audio system
    m_audio.reset();

#if defined(WITH_DIRECT_MANIPULATION)
    if (m_guiContext)
        m_guiContext->directManip.reset();
#endif

    // L6
    SingletonRegistry::destroy(m_inferController);

    // L5 (reverse)
    SingletonRegistry::destroy(m_projectPackageResolver);
    if (m_guiContext)
        SingletonRegistry::destroy(m_guiContext->projectStatusInstance);
    SingletonRegistry::destroy(m_playbackController);

    // L4 (reverse)
    SingletonRegistry::destroy(m_editSessionManager);
    if (m_guiContext) {
        SingletonRegistry::destroy(m_guiContext->midiExtractInstance);
        SingletonRegistry::destroy(m_guiContext->pitchExtractInstance);
        SingletonRegistry::destroy(m_guiContext->undoRedoInstance);
        SingletonRegistry::destroy(m_guiContext->editorViewInstance);
        SingletonRegistry::destroy(m_guiContext->clipInstance);
        SingletonRegistry::destroy(m_guiContext->trackInstance);
        SingletonRegistry::destroy(m_guiContext->clipboardInstance);
    }
    SingletonRegistry::destroy(m_audioDecodingController);

    // Runtime-owned workers must stop before the services they use are destroyed.
    m_coreRuntime.reset();

    // L3
    SingletonRegistry::destroy(m_inferEngine);
    SingletonRegistry::destroy(m_synthrtEngine);

    // Level meter manager (depends on AppModel, must die before L0)
    if (m_guiContext)
        SingletonRegistry::destroy(m_guiContext->levelMeterInstance);
    m_guiContext.reset();

    // L1 (reverse)
    SingletonRegistry::destroy(m_packageManager);
    SingletonRegistry::destroy(m_historyManager);

    // L0 (reverse)
    SingletonRegistry::destroy(m_paramUtils);
    SingletonRegistry::destroy(m_appModel);
    SingletonRegistry::destroy(m_appOptions);
    SingletonRegistry::destroy(m_appStatus);

    SingletonRegistry::clear();
    s_self = nullptr;
}

// instance<T>() specializations — one per business singleton
// Returns nullptr when s_self is not set (e.g. in tests); callers fall back to Meyers static.
template <>
AppStatus *AppContext::instance() {
    return s_self ? s_self->m_appStatus : nullptr;
}

template <>
AppOptions *AppContext::instance() {
    return s_self ? s_self->m_appOptions : nullptr;
}

template <>
AppModel *AppContext::instance() {
    return s_self ? s_self->m_appModel : nullptr;
}

template <>
ParamUtils *AppContext::instance() {
    return s_self ? s_self->m_paramUtils : nullptr;
}

template <>
HistoryManager *AppContext::instance() {
    return s_self ? s_self->m_historyManager : nullptr;
}

template <>
PackageManager *AppContext::instance() {
    return s_self ? s_self->m_packageManager : nullptr;
}

template <>
LangSetting::ILangSetManager *AppContext::instance() {
    return s_self ? s_self->m_iLangSetManager : nullptr;
}

template <>
SynthrtEngine *AppContext::instance() {
    return s_self ? s_self->m_synthrtEngine : nullptr;
}

template <>
InferEngine *AppContext::instance() {
    return s_self ? s_self->m_inferEngine : nullptr;
}

template <>
AudioDecodingController *AppContext::instance() {
    return s_self ? s_self->m_audioDecodingController : nullptr;
}

template <>
ClipboardController *AppContext::instance() {
    return s_self && s_self->m_guiContext ? s_self->m_guiContext->clipboardInstance : nullptr;
}

template <>
TrackController *AppContext::instance() {
    return s_self && s_self->m_guiContext ? s_self->m_guiContext->trackInstance : nullptr;
}

template <>
ClipController *AppContext::instance() {
    return s_self && s_self->m_guiContext ? s_self->m_guiContext->clipInstance : nullptr;
}

template <>
EditorViewController *AppContext::instance() {
    return s_self && s_self->m_guiContext ? s_self->m_guiContext->editorViewInstance : nullptr;
}

template <>
UndoRedoController *AppContext::instance() {
    return s_self && s_self->m_guiContext ? s_self->m_guiContext->undoRedoInstance : nullptr;
}

template <>
PitchExtractController *AppContext::instance() {
    return s_self && s_self->m_guiContext ? s_self->m_guiContext->pitchExtractInstance : nullptr;
}

template <>
MidiExtractController *AppContext::instance() {
    return s_self && s_self->m_guiContext ? s_self->m_guiContext->midiExtractInstance : nullptr;
}

template <>
EditSessionManager *AppContext::instance() {
    return s_self ? s_self->m_editSessionManager : nullptr;
}

template <>
PlaybackController *AppContext::instance() {
    return s_self ? s_self->m_playbackController : nullptr;
}

template <>
ProjectStatusController *AppContext::instance() {
    return s_self && s_self->m_guiContext ? s_self->m_guiContext->projectStatusInstance : nullptr;
}

template <>
ProjectPackageResolver *AppContext::instance() {
    return s_self ? s_self->m_projectPackageResolver : nullptr;
}

template <>
InferController *AppContext::instance() {
    return s_self ? s_self->m_inferController : nullptr;
}

template <>
AppController *AppContext::instance() {
    return s_self && s_self->m_guiContext ? s_self->m_guiContext->appInstance : nullptr;
}

template <>
DocumentWorkflowController *AppContext::instance() {
    return s_self && s_self->m_guiContext ? s_self->m_guiContext->documentWorkflowInstance
                                          : nullptr;
}

template <>
LevelMeterManager *AppContext::instance() {
    return s_self && s_self->m_guiContext ? s_self->m_guiContext->levelMeterInstance : nullptr;
}

template <>
Automation::CoreRuntime *AppContext::instance() {
    return s_self ? s_self->m_coreRuntime.get() : nullptr;
}
