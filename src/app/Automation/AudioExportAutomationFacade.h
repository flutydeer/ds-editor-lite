#ifndef AUDIOEXPORTAUTOMATIONFACADE_H
#define AUDIOEXPORTAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"
#include "AutomationTaskManager.h"
#include "SettingsAutomationFacade.h"

#include <QHash>
#include <QMutex>

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

    struct AudioExportCapabilitiesDto {
        QStringList formats;
        QList<int> sampleRates;
        QStringList channelModes;
        QStringList mixingModes;
        QStringList sourceModes;
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
        // Emitted while the backend waits for pending pieces to render, before the
        // actual audio export starts. progress in [0, 1] over renderable pieces.
        std::function<void(double progress)> inferenceProgress;
    };

    class IAudioExportJob {
    public:
        virtual ~IAudioExportJob() = default;

        [[nodiscard]] virtual AudioExportPreviewDto preview() const = 0;
        virtual AudioExportBackendResult waitUntilReady(const AudioExportObserver &observer) = 0;
        virtual AudioExportBackendResult execute(const AudioExportObserver &observer,
                                                 bool deferPublish) = 0;
        virtual AudioExportBackendResult publish(const AudioExportObserver &observer,
                                                 bool allowOverwrite) = 0;
        virtual void cancel() = 0;
        virtual void cleanup() = 0;
    };

    struct AudioExportRuntimeServices {
        std::function<AutomationResult<std::shared_ptr<IAudioExportJob>>(
            AppModel *, const QString &projectPath, const AudioExportConfigDto &)>
            createJob;
        std::function<void(std::function<void()>)> schedule;
    };

    using AudioExportAccessRevalidator = std::function<AutomationResult<AutomationUnit>()>;
    using AudioExportOutputAuthorizer =
        std::function<AutomationResult<AutomationUnit>(const AudioExportPreviewDto &)>;

    class AudioExportAutomationFacade final {
    public:
        AudioExportAutomationFacade(AutomationDispatcher &dispatcher,
                                    IDocumentSessionResolver &documentResolver,
                                    AutomationTaskManager &tasks,
                                    AudioExportRuntimeServices services = {});

        AutomationResult<AudioExportPreviewDto> preview(const DocumentId &documentId,
                                                        const AudioExportConfigDto &config);
        AutomationResult<TaskAcceptedResult> start(const CommandContext &context,
                                                   const AudioExportConfigDto &config,
                                                   const AudioExportPolicyDto &policy,
                                                   AudioExportObserver observer = {});
        AutomationResult<TaskAcceptedResult> start(const CommandContext &context,
                                                   const AudioExportConfigDto &config,
                                                   const AudioExportPolicyDto &policy,
                                                   AudioExportObserver observer,
                                                   AudioExportAccessRevalidator reauthorize);
        AutomationResult<TaskAcceptedResult> start(const CommandContext &context,
                                                   const AudioExportConfigDto &config,
                                                   const AudioExportPolicyDto &policy,
                                                   AudioExportObserver observer,
                                                   AudioExportOutputAuthorizer authorizeOutputs,
                                                   AudioExportAccessRevalidator reauthorize);
        AutomationResult<ApplicationMutationResult> cleanup(const CommandContext &context,
                                                            const TaskId &taskId);

        [[nodiscard]] static AudioExportCapabilitiesDto capabilities();

        void discardDocumentGeneration(const DocumentId &documentId);

    private:
        struct PendingJobState;
        struct JobRecord;

        void executeTask(const TaskId &taskId, DocumentVersion baseDocument,
                         AudioExportObserver observer, AudioExportAccessRevalidator reauthorize,
                         const std::shared_ptr<PendingJobState> &state, bool allowOverwrite);
        AutomationResult<std::reference_wrapper<DocumentSession>>
            resolveDocumentVersion(const DocumentVersion &version) const;
        void removeJobRecord(const TaskId &taskId);

        AutomationDispatcher &m_dispatcher;
        IDocumentSessionResolver &m_documentResolver;
        AutomationTaskManager &m_tasks;
        AudioExportRuntimeServices m_services;
        QMutex m_jobsMutex;
        QHash<TaskId, std::shared_ptr<JobRecord>> m_jobs;
    };

} // namespace Automation

#endif // AUDIOEXPORTAUTOMATIONFACADE_H
