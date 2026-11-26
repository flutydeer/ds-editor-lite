#ifndef APPCONTEXT_H
#define APPCONTEXT_H

#include "Bootstrap/AppHostMode.h"
#include <lite/Support/ObjectContext.h>

#include <memory>

// Forward declarations — no heavy includes needed
class AppModel;
class AppOptions;
class AppStatus;
class ParamUtils;
class TaskManager;
class HistoryManager;
class PackageManager;

namespace LangSetting {
    class ILangSetManager;
}
class InferEngine;
class SynthrtEngine;
class AudioDecodingController;
class ClipboardController;
class TrackController;
class ClipController;
class EditorViewController;
class UndoRedoController;
class PitchExtractController;
class MidiExtractController;
class EditSessionManager;
class PlaybackController;
class ProjectStatusController;
class ProjectPackageResolver;
class InferController;
class AppController;
class DocumentWorkflowController;
class LevelMeterManager;
class AudioContext;

namespace Automation {
    class CoreRuntime;
}

struct AudioSystemContext;

class AppContext {
public:
    AppContext(const AppContext &) = delete;
    AppContext &operator=(const AppContext &) = delete;

    template <typename T>
    static T *instance() { return SingletonRegistry::instance<T>(); }

    // Public so main() can construct, but conceptually private to the app entry point.
    // Making it a friend of main() is not possible (main is a C runtime symbol).
    explicit AppContext(std::unique_ptr<AppOptions> options,
                        AppHostMode hostMode = AppHostMode::Gui);
    explicit AppContext(AppContext &process);
    ~AppContext();
    const SingletonRegistry::Context &services() const { return m_services; }

    [[nodiscard]] AppHostMode hostMode() const;
    [[nodiscard]] bool hasGui() const;
    bool initializeDefaultDocument(QString *error = nullptr);

    // Commits the host exit. Arms the process-lifetime watchdog first, then freezes
    // the document through the core runtime. Idempotent, so every exit path can call
    // it: the GUI terminationApproved handler, the headless termination request, the
    // composition-root aboutToQuit fallback and this context's destructor.
    void commitShutdown();
    [[nodiscard]] bool shutdownCommitted() const;

    // L0: Basic data models
    AppStatus *m_appStatus = nullptr;
    AppOptions *m_appOptions = nullptr;
    AppModel *m_appModel = nullptr;
    ParamUtils *m_paramUtils = nullptr;

    // L1: Independent modules
    HistoryManager *m_historyManager = nullptr;
    PackageManager *m_packageManager = nullptr;
    std::unique_ptr<Automation::CoreRuntime> m_coreRuntime;

    // L2: Language modules
    LangSetting::ILangSetManager *m_iLangSetManager = nullptr;

    // L3: Runtime host and inference engine
    SynthrtEngine *m_synthrtEngine = nullptr;
    InferEngine *m_inferEngine = nullptr;

    // L4: Core controllers (no construction-time cross-deps)
    AudioDecodingController *m_audioDecodingController = nullptr;
    EditSessionManager *m_editSessionManager = nullptr;

    // L5: Core controllers with construction-time deps
    PlaybackController *m_playbackController = nullptr;
    ProjectPackageResolver *m_projectPackageResolver = nullptr;

    // L6: Inference controller
    InferController *m_inferController = nullptr;

    // Audio system (existing, moved here)
    std::unique_ptr<AudioSystemContext> m_audio;

    static AppContext *s_self;

private:
    struct GuiContext;
    AppContext(std::unique_ptr<AppOptions> options, AppHostMode hostMode, AppContext *process);
    SingletonRegistry::Context m_services;
    AppContext *m_process;
    std::unique_ptr<AudioContext> m_projectAudio;

    void initializeCommonWiring();
    // Resets the document-scoped state of the host (playback, editor selection and
    // viewports) while a document is being replaced. See the contract in
    // docs/design/document-scoped-state-reset-design.md.
    void resetDocumentScopedState();
    [[nodiscard]] EditorViewController *guiEditorViewController() const;
    [[nodiscard]] DocumentWorkflowController *guiDocumentWorkflowController() const;

    AppHostMode m_hostMode;
    std::unique_ptr<GuiContext> m_guiContext;
    bool m_shutdownCommitted = false;
};

#endif // APPCONTEXT_H
