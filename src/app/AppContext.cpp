//
// Created by FlutyDeer on 2026/7/3.
//

#include "AppContext.h"

#include <lite/Core/Registry.h>

// Business singletons — all headers included here
#include "Model/AppModel/AppModel.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Model/Utils/ParamUtils.h"
#include <lite/Tasking/TaskManager.h>
#include "Modules/History/HistoryManager.h"
#include "Modules/PackageManager/PackageManager.h"
#include "Modules/SynthrtEngine/SynthrtEngine.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/InferController.h"
#include "Modules/Inference/EditSessionManager.h"
#include "Modules/Extractors/PitchExtractController.h"
#include "Modules/Extractors/MidiExtractController.h"
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
#include "UI/Controls/LevelMeterManager.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QThread>

#if defined(WITH_DIRECT_MANIPULATION)
#include <QWDMHCore/DirectManipulationSystem.h>
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

AppContext *AppContext::s_self = nullptr;

AppContext::AppContext(std::unique_ptr<AppOptions> options) {
    s_self = this;

    // Registry::create constructs each service (bypassing its private ctor via
    // the Registry friendship) and registers it immediately, so that a later
    // service's constructor — which may call Xxx::instance() — resolves the
    // owned instance instead of falling back to a stray Meyers static. This
    // object still owns the returned pointers and tears them down, in reverse,
    // via Registry::destroy in the destructor.

    // L0: Basic data models (no dependencies)
    m_appStatus = Registry::create<AppStatus>();
    // AppOptions is constructed by main() and handed in; just register it.
    m_appOptions = options.release();
    Registry::add(m_appOptions);
    m_appModel = Registry::create<AppModel>();
    m_paramUtils = Registry::create<ParamUtils>();

    // L1: Independent modules
    TaskManager::instance(); // force early construction on the main thread
    m_historyManager = Registry::create<HistoryManager>();
    m_packageManager = Registry::create<PackageManager>();

    // L3: Runtime host must outlive the inference facade.
    m_synthrtEngine = Registry::create<SynthrtEngine>();
    m_inferEngine = Registry::create<InferEngine>();
    m_inferEngine->startInitialization();

    // Level meter manager (depends on AppModel from L0)
    m_levelMeterManager = Registry::create<LevelMeterManager>(m_appModel);

    // L4: Controllers (no construction-time cross-deps)
    m_audioDecodingController = Registry::create<AudioDecodingController>();
    m_clipboardController = Registry::create<ClipboardController>();
    m_trackController = Registry::create<TrackController>();
    m_clipController = Registry::create<ClipController>();
    m_editorViewController = Registry::create<EditorViewController>();
    m_undoRedoController = Registry::create<UndoRedoController>();
    m_pitchExtractController = Registry::create<PitchExtractController>();
    m_midiExtractController = Registry::create<MidiExtractController>();
    m_editSessionManager = Registry::create<EditSessionManager>();

    // L5: Controllers with construction-time deps
    m_playbackController = Registry::create<PlaybackController>();
    m_projectStatusController = Registry::create<ProjectStatusController>();
    // ProjectPackageResolver connects to AppModel + PackageManager + AppStatus
    m_projectPackageResolver = Registry::create<ProjectPackageResolver>();

    // L6: InferController connects to AppOptions, AppStatus, EditSessionManager, PlaybackController
    m_inferController = Registry::create<InferController>();

    // Audio system (replaces old AudioSystemContext)
    m_audio = std::make_unique<AudioSystemContext>();

#if defined(WITH_DIRECT_MANIPULATION)
    m_directManip = std::make_unique<DirectManipulationHolder>();
#endif

    // L7: AppController (constructs last, destructs first)
    // Its constructor calls initializeModules() which triggers instance() calls
    // to InferEngine, ProjectPackageResolver, InferController, etc.
    // — all already constructed above, so instance() will return valid pointers.
    m_appController = Registry::create<AppController>();
    m_documentWorkflowController = Registry::create<DocumentWorkflowController>();
}

AppContext::~AppContext() {
    // Reverse order of construction.
    Registry::destroy(m_documentWorkflowController);

    // L7: AppController dies while MainWindow is still on the stack.
    Registry::destroy(m_appController);

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
    m_directManip.reset();
#endif

    // L6
    Registry::destroy(m_inferController);

    // L5 (reverse)
    Registry::destroy(m_projectPackageResolver);
    Registry::destroy(m_projectStatusController);
    Registry::destroy(m_playbackController);

    // L4 (reverse)
    Registry::destroy(m_editSessionManager);
    Registry::destroy(m_midiExtractController);
    Registry::destroy(m_pitchExtractController);
    Registry::destroy(m_undoRedoController);
    Registry::destroy(m_editorViewController);
    Registry::destroy(m_clipController);
    Registry::destroy(m_trackController);
    Registry::destroy(m_clipboardController);
    Registry::destroy(m_audioDecodingController);

    // L3
    Registry::destroy(m_inferEngine);
    Registry::destroy(m_synthrtEngine);

    // Level meter manager (depends on AppModel, must die before L0)
    Registry::destroy(m_levelMeterManager);

    // L1 (reverse)
    Registry::destroy(m_packageManager);
    Registry::destroy(m_historyManager);

    // L0 (reverse)
    Registry::destroy(m_paramUtils);
    Registry::destroy(m_appModel);
    Registry::destroy(m_appOptions);
    Registry::destroy(m_appStatus);

    Registry::clear();
    s_self = nullptr;
}

// instance<T>() specializations — one per business singleton
// Returns nullptr when s_self is not set (e.g. in tests); callers fall back to Meyers static.
template <> AppStatus *AppContext::instance() { return s_self ? s_self->m_appStatus : nullptr; }
template <> AppOptions *AppContext::instance() { return s_self ? s_self->m_appOptions : nullptr; }
template <> AppModel *AppContext::instance() { return s_self ? s_self->m_appModel : nullptr; }
template <> ParamUtils *AppContext::instance() { return s_self ? s_self->m_paramUtils : nullptr; }
template <> HistoryManager *AppContext::instance() { return s_self ? s_self->m_historyManager : nullptr; }
template <> PackageManager *AppContext::instance() { return s_self ? s_self->m_packageManager : nullptr; }
template <> LangSetting::ILangSetManager *AppContext::instance() { return s_self ? s_self->m_iLangSetManager : nullptr; }
template <> SynthrtEngine *AppContext::instance() { return s_self ? s_self->m_synthrtEngine : nullptr; }
template <> InferEngine *AppContext::instance() { return s_self ? s_self->m_inferEngine : nullptr; }
template <> AudioDecodingController *AppContext::instance() { return s_self ? s_self->m_audioDecodingController : nullptr; }
template <> ClipboardController *AppContext::instance() { return s_self ? s_self->m_clipboardController : nullptr; }
template <> TrackController *AppContext::instance() { return s_self ? s_self->m_trackController : nullptr; }
template <> ClipController *AppContext::instance() { return s_self ? s_self->m_clipController : nullptr; }
template <> EditorViewController *AppContext::instance() { return s_self ? s_self->m_editorViewController : nullptr; }
template <> UndoRedoController *AppContext::instance() { return s_self ? s_self->m_undoRedoController : nullptr; }
template <> PitchExtractController *AppContext::instance() { return s_self ? s_self->m_pitchExtractController : nullptr; }
template <> MidiExtractController *AppContext::instance() { return s_self ? s_self->m_midiExtractController : nullptr; }
template <> EditSessionManager *AppContext::instance() { return s_self ? s_self->m_editSessionManager : nullptr; }
template <> PlaybackController *AppContext::instance() { return s_self ? s_self->m_playbackController : nullptr; }
template <> ProjectStatusController *AppContext::instance() { return s_self ? s_self->m_projectStatusController : nullptr; }
template <> ProjectPackageResolver *AppContext::instance() { return s_self ? s_self->m_projectPackageResolver : nullptr; }
template <> InferController *AppContext::instance() { return s_self ? s_self->m_inferController : nullptr; }
template <> AppController *AppContext::instance() { return s_self ? s_self->m_appController : nullptr; }
template <> DocumentWorkflowController *AppContext::instance() { return s_self ? s_self->m_documentWorkflowController : nullptr; }
template <> LevelMeterManager *AppContext::instance() { return s_self ? s_self->m_levelMeterManager : nullptr; }
