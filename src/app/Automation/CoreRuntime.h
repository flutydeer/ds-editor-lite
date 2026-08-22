#ifndef CORERUNTIME_H
#define CORERUNTIME_H

#include "DocumentSessionResolver.h"
#include "DocumentAutomationFacade.h"
#include "EditorAutomationFacade.h"
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
        CoreRuntime(AppModel *model,
                    HistoryManager *historyManager,
                    DocumentRuntimeServices documentServices = {},
                    PlaybackRuntimeServices playbackServices = {},
                    EditorRuntimeServices editorServices = {},
                    SettingsRuntimeServices settingsServices = {},
                    PresetRuntimeServices presetServices = {},
                    PackageRuntimeServices packageServices = {},
                    InferenceRuntimeServices inferenceServices = {},
                    FileRuntimeServices fileServices = {});

        [[nodiscard]] DocumentVersion documentVersion() const;
        [[nodiscard]] const WindowId &windowId() const;

        EditorAutomationFacade &facade();
        const EditorAutomationFacade &facade() const;
        OperationCatalog &catalog();
        const OperationCatalog &catalog() const;
        AutomationDispatcher &dispatcher();
        DocumentAutomationFacade &documents();
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

        DocumentVersion replaceDocumentGeneration(QString path, QString projectName);
        bool setDocumentBusy(const DocumentId &documentId, bool busy);
        bool setDocumentIdentity(const DocumentId &documentId, QString path, QString projectName);

    private:
        DocumentSession m_session;
        SingleDocumentSessionResolver m_documentResolver;
        SingleWindowContext m_windowContext;
        OperationCatalog m_catalog;
        AutomationDispatcher m_dispatcher;
        CommandCommitter m_committer;
        DocumentObjectResolver m_objectResolver;
        AutomationTaskManager m_taskManager;
        DocumentAutomationFacade m_documentFacade;
        EditorAutomationFacade m_facade;
        FileAutomationFacade m_fileFacade;
        HistoryAutomationFacade m_historyFacade;
        InferenceAutomationFacade m_inferenceFacade;
        NoteAutomationFacade m_noteFacade;
        PackageAutomationFacade m_packageFacade;
        ParameterAutomationFacade m_parameterFacade;
        PlaybackAutomationFacade m_playbackFacade;
        PresetAutomationFacade m_presetFacade;
        ProjectAutomationFacade m_projectFacade;
        SettingsAutomationFacade m_settingsFacade;
        TaskAutomationFacade m_taskFacade;
        TimelineAutomationFacade m_timelineFacade;
    };

} // namespace Automation

#endif // CORERUNTIME_H
