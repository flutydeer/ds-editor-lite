//
// Created by FlutyDeer on 2026/7/3.
//

#include "AppContext.h"

// Business singletons — all headers included here
#include "Model/AppModel/AppModel.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Utils/ParamUtils.h"
#include "Utils/IdGenerator.h"
#include "Modules/Task/TaskManager.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/PackageManager/PackageManager.h"
#include "Modules/Language/S2pMgr.h"
#include "Modules/Language/OnsetMarker/OnsetMarkerMgr.h"
#include "Modules/Language/LangSetting/ILangSetManager.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/InferController.h"
#include "Modules/Inference/EditSessionManager.h"
#include "Modules/Extractors/PitchExtractController.h"
#include "Modules/Extractors/MidiExtractController.h"
#include "Controller/AppController.h"
#include "Controller/AudioDecodingController.h"
#include "Controller/ClipboardController.h"
#include "Controller/TrackController.h"
#include "Controller/ClipController.h"
#include "Controller/PlaybackController.h"
#include "Controller/ValidationController.h"
#include "Controller/ProjectStatusController.h"
#include "Controller/ProjectPackageResolver.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/Audio/subsystem/MidiSystem.h"
#include "Modules/Audio/subsystem/OutputSystem.h"
#include "Modules/Audio/utils/DeviceTester.h"

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

AppContext::AppContext() {
    s_self = this;

    // L0: Basic data models (no dependencies)
    m_appStatus = new AppStatus;
    m_appOptions = new AppOptions;
    m_appModel = new AppModel;
    m_paramUtils = new ParamUtils;

    // L1: Independent modules
    m_idGenerator = new IdGenerator;
    m_taskManager = new TaskManager;
    m_historyManager = new HistoryManager;
    m_packageManager = new PackageManager;

    // L2: Language modules
    m_s2pMgr = new S2pMgr;
    m_onsetMarkerMgr = new OnsetMarkerMgr;
    m_iLangSetManager = new LangSetting::ILangSetManager;

    // L3: Inference engine (lazy init, no deps at construction)
    m_inferEngine = new InferEngine;

    // L4: Controllers (no construction-time cross-deps)
    m_audioDecodingController = new AudioDecodingController;
    m_clipboardController = new ClipboardController;
    m_trackController = new TrackController;
    m_clipController = new ClipController;
    m_pitchExtractController = new PitchExtractController;
    m_midiExtractController = new MidiExtractController;
    m_editSessionManager = new EditSessionManager;

    // L5: Controllers with construction-time deps
    // ValidationController connects to AppModel + HistoryManager (both L0/L1, already up)
    // PlaybackController connects to AppModel + ValidationController (need VC first)
    m_validationController = new ValidationController;
    m_playbackController = new PlaybackController;
    m_projectStatusController = new ProjectStatusController;
    // ProjectPackageResolver connects to AppModel + PackageManager + AppStatus
    m_projectPackageResolver = new ProjectPackageResolver;

    // L6: InferController connects to AppOptions, AppStatus, EditSessionManager, PlaybackController
    m_inferController = new InferController;

    // Audio system (replaces old AudioSystemContext)
    m_audio = std::make_unique<AudioSystemContext>();

#if defined(WITH_DIRECT_MANIPULATION)
    m_directManip = std::make_unique<DirectManipulationHolder>();
#endif

    // L7: AppController (constructs last, destructs first)
    // Its constructor calls initializeModules() which triggers instance() calls
    // to InferEngine, ProjectPackageResolver, InferController, etc.
    // — all already constructed above, so instance() will return valid pointers.
    m_appController = new AppController;
}

AppContext::~AppContext() {
    // Reverse order of construction.
    // L7: AppController first — dies while MainWindow is still on the stack.
    delete m_appController;

    // Audio system
    m_audio.reset();

#if defined(WITH_DIRECT_MANIPULATION)
    m_directManip.reset();
#endif

    // L6
    delete m_inferController;

    // L5 (reverse)
    delete m_projectPackageResolver;
    delete m_projectStatusController;
    delete m_playbackController;
    delete m_validationController;

    // L4 (reverse)
    delete m_editSessionManager;
    delete m_midiExtractController;
    delete m_pitchExtractController;
    delete m_clipController;
    delete m_trackController;
    delete m_clipboardController;
    delete m_audioDecodingController;

    // L3
    delete m_inferEngine;

    // L2 (reverse)
    delete m_iLangSetManager;
    delete m_onsetMarkerMgr;
    delete m_s2pMgr;

    // L1 (reverse)
    delete m_packageManager;
    delete m_historyManager;
    delete m_taskManager;
    delete m_idGenerator;

    // L0 (reverse)
    delete m_paramUtils;
    delete m_appModel;
    delete m_appOptions;
    delete m_appStatus;

    s_self = nullptr;
}

// instance<T>() specializations — one per business singleton
// Returns nullptr when s_self is not set (e.g. in tests); callers fall back to Meyers static.
template <> AppStatus *AppContext::instance() { return s_self ? s_self->m_appStatus : nullptr; }
template <> AppOptions *AppContext::instance() { return s_self ? s_self->m_appOptions : nullptr; }
template <> AppModel *AppContext::instance() { return s_self ? s_self->m_appModel : nullptr; }
template <> ParamUtils *AppContext::instance() { return s_self ? s_self->m_paramUtils : nullptr; }
template <> IdGenerator *AppContext::instance() { return s_self ? s_self->m_idGenerator : nullptr; }
template <> TaskManager *AppContext::instance() { return s_self ? s_self->m_taskManager : nullptr; }
template <> HistoryManager *AppContext::instance() { return s_self ? s_self->m_historyManager : nullptr; }
template <> PackageManager *AppContext::instance() { return s_self ? s_self->m_packageManager : nullptr; }
template <> S2pMgr *AppContext::instance() { return s_self ? s_self->m_s2pMgr : nullptr; }
template <> OnsetMarkerMgr *AppContext::instance() { return s_self ? s_self->m_onsetMarkerMgr : nullptr; }
template <> LangSetting::ILangSetManager *AppContext::instance() { return s_self ? s_self->m_iLangSetManager : nullptr; }
template <> InferEngine *AppContext::instance() { return s_self ? s_self->m_inferEngine : nullptr; }
template <> AudioDecodingController *AppContext::instance() { return s_self ? s_self->m_audioDecodingController : nullptr; }
template <> ClipboardController *AppContext::instance() { return s_self ? s_self->m_clipboardController : nullptr; }
template <> TrackController *AppContext::instance() { return s_self ? s_self->m_trackController : nullptr; }
template <> ClipController *AppContext::instance() { return s_self ? s_self->m_clipController : nullptr; }
template <> PitchExtractController *AppContext::instance() { return s_self ? s_self->m_pitchExtractController : nullptr; }
template <> MidiExtractController *AppContext::instance() { return s_self ? s_self->m_midiExtractController : nullptr; }
template <> EditSessionManager *AppContext::instance() { return s_self ? s_self->m_editSessionManager : nullptr; }
template <> ValidationController *AppContext::instance() { return s_self ? s_self->m_validationController : nullptr; }
template <> PlaybackController *AppContext::instance() { return s_self ? s_self->m_playbackController : nullptr; }
template <> ProjectStatusController *AppContext::instance() { return s_self ? s_self->m_projectStatusController : nullptr; }
template <> ProjectPackageResolver *AppContext::instance() { return s_self ? s_self->m_projectPackageResolver : nullptr; }
template <> InferController *AppContext::instance() { return s_self ? s_self->m_inferController : nullptr; }
template <> AppController *AppContext::instance() { return s_self ? s_self->m_appController : nullptr; }

// Infrastructure singletons — NOT managed by AppContext (stays Meyers static).
// These specializations return nullptr so the LITE_SINGLETON_IMPLEMENT_INSTANCE fallback
// to Meyers static kicks in.
class Toast;
class AppColorPalette;
class TextPixmapCache;
class ThemeManager;
class Log;
template <> Toast *AppContext::instance() { return nullptr; }
template <> AppColorPalette *AppContext::instance() { return nullptr; }
template <> TextPixmapCache *AppContext::instance() { return nullptr; }
template <> ThemeManager *AppContext::instance() { return nullptr; }
template <> Log *AppContext::instance() { return nullptr; }
