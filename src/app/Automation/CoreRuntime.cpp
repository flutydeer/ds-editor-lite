#include "CoreRuntime.h"

namespace Automation {
    namespace {
        DocumentRuntimeServices bindGenerationLifecycle(DocumentRuntimeServices services,
                                                        AudioExportAutomationFacade *audioExports,
                                                        ExtractionAutomationFacade *extractions) {
            auto existing = std::move(services.beforeReplaceGeneration);
            services.beforeReplaceGeneration = [existing = std::move(existing), audioExports,
                                                extractions](const DocumentId &documentId) {
                if (existing)
                    existing(documentId);
                audioExports->discardDocumentGeneration(documentId);
                extractions->discardDocumentGeneration(documentId);
            };
            return services;
        }
    }

    CoreRuntime::CoreRuntime(
        AppModel *model, HistoryManager *historyManager, DocumentRuntimeServices documentServices,
        PlaybackRuntimeServices playbackServices, EditorRuntimeServices editorServices,
        SettingsRuntimeServices settingsServices, PresetRuntimeServices presetServices,
        PackageRuntimeServices packageServices, InferenceRuntimeServices inferenceServices,
        FileRuntimeServices fileServices, AudioExportRuntimeServices audioExportServices,
        ExtractionRuntimeServices extractionServices,
        ApplicationRuntimeServices applicationServices)
        : m_session(model, historyManager), m_documentResolver(m_session),
          m_dispatcher(m_documentResolver, m_windowContext, m_catalog),
          m_applicationFacade(m_catalog, m_dispatcher, std::move(applicationServices)),
          m_parameterFacade(m_catalog, m_dispatcher, m_committer, m_objectResolver),
          m_projectFacade(m_catalog, m_dispatcher, m_committer, m_objectResolver),
          m_audioExportFacade(m_catalog, m_dispatcher, m_documentResolver, m_taskManager,
                              std::move(audioExportServices)),
          m_extractionFacade(m_catalog, m_dispatcher, m_taskManager, m_objectResolver,
                             m_parameterFacade, m_projectFacade, std::move(extractionServices)),
          m_documentFacade(m_catalog, m_dispatcher, m_committer, m_taskManager,
                           bindGenerationLifecycle(std::move(documentServices),
                                                   &m_audioExportFacade, &m_extractionFacade)),
          m_facade(m_catalog, m_dispatcher, m_objectResolver, std::move(editorServices)),
          m_fileFacade(m_catalog, m_dispatcher, std::move(fileServices)),
          m_historyFacade(m_catalog, m_dispatcher, m_committer),
          m_inferenceFacade(m_catalog, m_dispatcher, m_committer, std::move(inferenceServices)),
          m_noteFacade(m_catalog, m_dispatcher, m_committer, m_objectResolver),
          m_packageFacade(m_catalog, m_dispatcher, std::move(packageServices)),
          m_playbackFacade(m_catalog, m_dispatcher, m_committer, std::move(playbackServices)),
          m_presetFacade(m_catalog, m_dispatcher, std::move(presetServices)),
          m_settingsFacade(m_catalog, m_dispatcher, std::move(settingsServices)),
          m_taskFacade(m_catalog, m_dispatcher, m_taskManager),
          m_timelineFacade(m_catalog, m_dispatcher, m_committer) {
    }

    DocumentVersion CoreRuntime::documentVersion() const {
        return m_session.version();
    }

    const WindowId &CoreRuntime::windowId() const {
        return m_windowContext.windowId();
    }

    EditorAutomationFacade &CoreRuntime::facade() {
        return m_facade;
    }

    const EditorAutomationFacade &CoreRuntime::facade() const {
        return m_facade;
    }

    OperationCatalog &CoreRuntime::catalog() {
        return m_catalog;
    }

    const OperationCatalog &CoreRuntime::catalog() const {
        return m_catalog;
    }

    AutomationDispatcher &CoreRuntime::dispatcher() {
        return m_dispatcher;
    }

    ApplicationAutomationFacade &CoreRuntime::application() {
        return m_applicationFacade;
    }

    AudioExportAutomationFacade &CoreRuntime::audioExports() {
        return m_audioExportFacade;
    }

    DocumentAutomationFacade &CoreRuntime::documents() {
        return m_documentFacade;
    }

    ExtractionAutomationFacade &CoreRuntime::extractions() {
        return m_extractionFacade;
    }

    FileAutomationFacade &CoreRuntime::files() {
        return m_fileFacade;
    }

    HistoryAutomationFacade &CoreRuntime::history() {
        return m_historyFacade;
    }

    InferenceAutomationFacade &CoreRuntime::inference() {
        return m_inferenceFacade;
    }

    NoteAutomationFacade &CoreRuntime::notes() {
        return m_noteFacade;
    }

    PackageAutomationFacade &CoreRuntime::packages() {
        return m_packageFacade;
    }

    ParameterAutomationFacade &CoreRuntime::parameters() {
        return m_parameterFacade;
    }

    PlaybackAutomationFacade &CoreRuntime::playback() {
        return m_playbackFacade;
    }

    PresetAutomationFacade &CoreRuntime::presets() {
        return m_presetFacade;
    }

    ProjectAutomationFacade &CoreRuntime::project() {
        return m_projectFacade;
    }

    SettingsAutomationFacade &CoreRuntime::settings() {
        return m_settingsFacade;
    }

    TaskAutomationFacade &CoreRuntime::tasks() {
        return m_taskFacade;
    }

    AutomationTaskManager &CoreRuntime::automationTasks() {
        return m_taskManager;
    }

    TimelineAutomationFacade &CoreRuntime::timeline() {
        return m_timelineFacade;
    }

    bool CoreRuntime::setDocumentBusy(const DocumentId &documentId, const bool busy) {
        if (documentId != m_session.documentId())
            return false;
        m_session.setBusy(busy);
        return true;
    }

} // namespace Automation
