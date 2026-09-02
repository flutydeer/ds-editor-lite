#ifndef CORERUNTIME_H
#define CORERUNTIME_H

#include "ApplicationAutomationFacade.h"
#include "AudioExportAutomationFacade.h"
#include "DocumentSessionResolver.h"
#include "DocumentAutomationFacade.h"
#include "EditorAutomationFacade.h"
#include "ExtractionAutomationFacade.h"
#include "FileAutomationFacade.h"
#include "HistoryAutomationFacade.h"
#include "InferenceAutomationFacade.h"
#include "NoteAutomationFacade.h"
#include "PackageAutomationFacade.h"
#include "ParameterAutomationFacade.h"
#include "PlaybackAutomationFacade.h"
#include "PresetAutomationFacade.h"
#include "ProjectAutomationFacade.h"
#include "SettingsAutomationFacade.h"
#include "TimelineAutomationFacade.h"
#include "TaskAutomationFacade.h"

class AppModel;
class HistoryManager;

namespace Automation {

    class CoreRuntime final {
    public:
        CoreRuntime(AppModel *model, HistoryManager *historyManager,
                    DocumentRuntimeServices documentServices = {},
                    PlaybackRuntimeServices playbackServices = {},
                    EditorRuntimeServices editorServices = {},
                    SettingsRuntimeServices settingsServices = {},
                    PresetRuntimeServices presetServices = {},
                    PackageRuntimeServices packageServices = {},
                    InferenceRuntimeServices inferenceServices = {},
                    FileRuntimeServices fileServices = {},
                    AudioExportRuntimeServices audioExportServices = {},
                    ExtractionRuntimeServices extractionServices = {},
                    ApplicationRuntimeServices applicationServices = {},
                    std::optional<WindowId> windowId = WindowId::create());

        [[nodiscard]] DocumentVersion documentVersion() const;
        [[nodiscard]] const QString &documentPath() const;
        [[nodiscard]] bool documentBusy(const DocumentId &documentId) const;
        [[nodiscard]] AutomationResult<CommandContext>
            documentWorkflowCommitContext(const DocumentVersion &generationAnchor) const;
        [[nodiscard]] AutomationResult<CommandContext>
            derivedWritebackContext(const DocumentVersion &taskVersion, bool validateOnly) const;
        [[nodiscard]] const std::optional<WindowId> &windowId() const;

        EditorAutomationFacade &facade();
        const EditorAutomationFacade &facade() const;
        AutomationDispatcher &dispatcher();
        ApplicationAutomationFacade &application();
        AudioExportAutomationFacade &audioExports();
        DocumentAutomationFacade &documents();
        ExtractionAutomationFacade &extractions();
        FileAutomationFacade &files();
        HistoryAutomationFacade &history();
        InferenceAutomationFacade &inference();
        NoteAutomationFacade &notes();
        PackageAutomationFacade &packages();
        ParameterAutomationFacade &parameters();
        PlaybackAutomationFacade &playback();
        PresetAutomationFacade &presets();
        ProjectAutomationFacade &project();
        SettingsAutomationFacade &settings();
        TaskAutomationFacade &tasks();
        AutomationTaskManager &automationTasks();
        TimelineAutomationFacade &timeline();

        bool setDocumentBusy(const DocumentId &documentId, bool busy);

    private:
        DocumentSession m_session;
        // 工作流租约由启动它的 generation 持有，换代后仍由该 generation 释放。
        DocumentId m_documentBusyOwner;
        SingleDocumentSessionResolver m_documentResolver;
        SingleWindowContext m_windowContext;
        AutomationDispatcher m_dispatcher;
        CommandCommitter m_committer;
        DocumentObjectResolver m_objectResolver;
        AutomationTaskManager m_taskManager;
        ApplicationAutomationFacade m_applicationFacade;
        ParameterAutomationFacade m_parameterFacade;
        ProjectAutomationFacade m_projectFacade;
        NoteAutomationFacade m_noteFacade;
        AudioExportAutomationFacade m_audioExportFacade;
        ExtractionAutomationFacade m_extractionFacade;
        DocumentAutomationFacade m_documentFacade;
        EditorAutomationFacade m_facade;
        FileAutomationFacade m_fileFacade;
        HistoryAutomationFacade m_historyFacade;
        InferenceAutomationFacade m_inferenceFacade;
        PackageAutomationFacade m_packageFacade;
        PlaybackAutomationFacade m_playbackFacade;
        PresetAutomationFacade m_presetFacade;
        SettingsAutomationFacade m_settingsFacade;
        TaskAutomationFacade m_taskFacade;
        TimelineAutomationFacade m_timelineFacade;
    };

} // namespace Automation

#endif // CORERUNTIME_H
