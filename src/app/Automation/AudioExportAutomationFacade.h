#ifndef AUDIOEXPORTAUTOMATIONFACADE_H
#define AUDIOEXPORTAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"
#include "AutomationTaskManager.h"
#include "SettingsAutomationFacade.h"

#include <QHash>

#include <functional>
#include <memory>

class AppModel;

namespace Automation {

    enum AudioExportWarningFlag : quint32 {
        AudioExportNoFile = 0x0001,
        AudioExportDuplicatedFile = 0x0002,
        AudioExportWillOverwrite = 0x0004,
        AudioExportUnrecognizedTemplate = 0x0008,
        AudioExportLossyFormat = 0x0010,
    };

    struct AudioExportPreviewDto {
        QString baseDirectory;
        QStringList filePaths;
        quint32 warningFlags = 0;

        friend bool operator==(const AudioExportPreviewDto &,
                               const AudioExportPreviewDto &) = default;
    };

    struct AudioExportPolicyDto {
        bool allowNoFiles = false;
        bool allowDuplicatePaths = false;
        bool allowOverwrite = false;
        bool allowUnrecognizedTemplate = false;
        bool allowLossyFormat = false;

        friend bool operator==(const AudioExportPolicyDto &,
                               const AudioExportPolicyDto &) = default;
    };

    enum class AudioExportBackendState {
        Succeeded,
        Failed,
        Canceled,
    };

    struct AudioExportBackendResult {
        AudioExportBackendState state = AudioExportBackendState::Failed;
        QString errorMessage;
    };

    struct AudioExportObserver {
        std::function<void(double progress, int sourceIndex)> progress;
        std::function<void(int sourceIndex)> clipping;
        std::function<void(const QString &message, int sourceIndex)> warning;
    };

    class IAudioExportJob {
    public:
        virtual ~IAudioExportJob() = default;

        [[nodiscard]] virtual AudioExportPreviewDto preview() const = 0;
        virtual AudioExportBackendResult execute(const AudioExportObserver &observer) = 0;
        virtual void cancel() = 0;
        virtual void cleanup() = 0;
    };

    struct AudioExportRuntimeServices {
        std::function<AutomationResult<std::shared_ptr<IAudioExportJob>>(
            AppModel *, const QString &projectPath, const AudioExportConfigDto &)>
            createJob;
        std::function<void(std::function<void()>)> schedule;
    };

    class AudioExportAutomationFacade final {
    public:
        AudioExportAutomationFacade(OperationCatalog &catalog, AutomationDispatcher &dispatcher,
                                    IDocumentSessionResolver &documentResolver,
                                    AutomationTaskManager &tasks,
                                    AudioExportRuntimeServices services = {});

        AutomationResult<AudioExportPreviewDto> preview(const DocumentId &documentId,
                                                        const AudioExportConfigDto &config);
        AutomationResult<TaskAcceptedResult> start(const CommandContext &context,
                                                   const AudioExportConfigDto &config,
                                                   const AudioExportPolicyDto &policy,
                                                   AudioExportObserver observer = {});
        AutomationResult<ApplicationMutationResult> cleanup(const CommandContext &context,
                                                            const TaskId &taskId);

        void discardDocumentGeneration(const DocumentId &documentId);

    private:
        struct PendingJobState;
        struct JobRecord;

        void registerOperations();
        void executeTask(const TaskId &taskId, DocumentVersion baseDocument,
                         AudioExportConfigDto config, AudioExportObserver observer,
                         const std::shared_ptr<PendingJobState> &state);
        AutomationResult<std::reference_wrapper<DocumentSession>>
            resolveVersion(const DocumentVersion &version) const;

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        IDocumentSessionResolver &m_documentResolver;
        AutomationTaskManager &m_tasks;
        AudioExportRuntimeServices m_services;
        QHash<TaskId, std::shared_ptr<JobRecord>> m_jobs;
    };

} // namespace Automation

#endif // AUDIOEXPORTAUTOMATIONFACADE_H
