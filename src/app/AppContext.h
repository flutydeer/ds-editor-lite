//
// Created by FlutyDeer on 2026/7/3.
//

#ifndef APPCONTEXT_H
#define APPCONTEXT_H

#include <memory>

// Forward declarations — no heavy includes needed
class AppModel;
class AppOptions;
class AppStatus;
class ParamUtils;
class TaskManager;
class HistoryManager;
class PackageManager;
class IdGenerator;
class S2pMgr;
class OnsetMarkerMgr;
namespace LangSetting { class ILangSetManager; }
class InferEngine;
class AudioDecodingController;
class ClipboardController;
class TrackController;
class ClipController;
class PitchExtractController;
class MidiExtractController;
class EditSessionManager;
class PlaybackController;
class ProjectStatusController;
class ProjectPackageResolver;
class InferController;
class AppController;
class LevelMeterManager;

struct AudioSystemContext;

class AppContext {
public:
    AppContext(const AppContext &) = delete;
    AppContext &operator=(const AppContext &) = delete;

    template <typename T>
    static T *instance();  // Returns nullptr if AppContext is not constructed

    // Public so main() can construct, but conceptually private to the app entry point.
    // Making it a friend of main() is not possible (main is a C runtime symbol).
    AppContext();
    ~AppContext();

    // L0: Basic data models
    AppStatus *m_appStatus = nullptr;
    AppOptions *m_appOptions = nullptr;
    AppModel *m_appModel = nullptr;
    ParamUtils *m_paramUtils = nullptr;

    // L1: Independent modules
    IdGenerator *m_idGenerator = nullptr;
    TaskManager *m_taskManager = nullptr;
    HistoryManager *m_historyManager = nullptr;
    PackageManager *m_packageManager = nullptr;

    // L2: Language modules
    S2pMgr *m_s2pMgr = nullptr;
    OnsetMarkerMgr *m_onsetMarkerMgr = nullptr;
    LangSetting::ILangSetManager *m_iLangSetManager = nullptr;

    // L3: Inference engine
    InferEngine *m_inferEngine = nullptr;

    // Level meter manager (depends on AppModel from L0)
    LevelMeterManager *m_levelMeterManager = nullptr;

    // L4: Controllers (no construction-time cross-deps)
    AudioDecodingController *m_audioDecodingController = nullptr;
    ClipboardController *m_clipboardController = nullptr;
    TrackController *m_trackController = nullptr;
    ClipController *m_clipController = nullptr;
    PitchExtractController *m_pitchExtractController = nullptr;
    MidiExtractController *m_midiExtractController = nullptr;
    EditSessionManager *m_editSessionManager = nullptr;

    // L5: Controllers with construction-time deps
    PlaybackController *m_playbackController = nullptr;
    ProjectStatusController *m_projectStatusController = nullptr;
    ProjectPackageResolver *m_projectPackageResolver = nullptr;

    // L6: Inference controller
    InferController *m_inferController = nullptr;

    // Audio system (existing, moved here)
    std::unique_ptr<AudioSystemContext> m_audio;

#if defined(WITH_DIRECT_MANIPULATION)
    std::unique_ptr<struct DirectManipulationHolder> m_directManip;
#endif

    // L7: Top-level controller (last constructed, first destructed)
    AppController *m_appController = nullptr;

    static AppContext *s_self;
};

#endif // APPCONTEXT_H
