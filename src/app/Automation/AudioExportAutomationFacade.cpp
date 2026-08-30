#include "AudioExportAutomationFacade.h"
#include "OperationIds.h"

#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace Automation {
    namespace {
        constexpr auto selectedSourceOption = 1;
        constexpr auto loopSectionRange = 1;

        AutomationError unavailable() {
            AutomationError error;
            error.code = AutomationErrorCode::ModuleNotReady;
            error.message = QStringLiteral("Audio export services are unavailable");
            return error;
        }

        AutomationError taskError(AutomationError error, const TaskId &taskId) {
            if (error.operationId.isEmpty())
                error.operationId = OperationIds::exports::audio::start;
            if (!error.taskId)
                error.taskId = taskId;
            return error;
        }

        AutomationResult<AutomationUnit> validateConfig(const AudioExportConfigDto &config) {
            if (config.fileName.trimmed().isEmpty()) {
                AutomationError error;
                error.code = AutomationErrorCode::PathRequired;
                error.fieldPath = QStringLiteral("config.file_name");
                error.message = QStringLiteral("Audio export file name is required");
                return error;
            }
            if (config.fileType < 0 || config.fileType > 3) {
                return AutomationError::invalidArgument(
                    QStringLiteral("config.file_type"),
                    QStringLiteral("Audio export file type is invalid"));
            }
            if ((config.fileType == 0 && (config.formatOption < 0 || config.formatOption > 3)) ||
                (config.fileType == 1 && (config.formatOption < 0 || config.formatOption > 2))) {
                return AutomationError::invalidArgument(
                    QStringLiteral("config.format_option"),
                    QStringLiteral("Audio export format option is invalid"));
            }
            const QStringList extensions = {
                QStringLiteral("wav"),
                QStringLiteral("flac"),
                QStringLiteral("ogg"),
                QStringLiteral("mp3"),
            };
            if (QFileInfo(config.fileName).suffix().toLower() != extensions.at(config.fileType)) {
                AutomationError error;
                error.code = AutomationErrorCode::FormatUnsupported;
                error.fieldPath = QStringLiteral("config.file_name");
                error.message = QStringLiteral("Audio export file extension does not match type");
                return error;
            }
            if (config.formatQuality < 0 || config.formatQuality > 100) {
                return AutomationError::invalidArgument(
                    QStringLiteral("config.format_quality"),
                    QStringLiteral("Audio export quality must be between 0 and 100"));
            }
            if (!std::isfinite(config.sampleRate) || config.sampleRate <= 0.0) {
                return AutomationError::invalidArgument(
                    QStringLiteral("config.sample_rate"),
                    QStringLiteral("Audio export sample rate must be positive"));
            }
            if (config.mixingOption < 0 || config.mixingOption > 2) {
                return AutomationError::invalidArgument(
                    QStringLiteral("config.mixing_option"),
                    QStringLiteral("Audio export mixing option is invalid"));
            }
            if (config.sourceOption < 0 || config.sourceOption > 2) {
                return AutomationError::invalidArgument(
                    QStringLiteral("config.source_option"),
                    QStringLiteral("Audio export source option is invalid"));
            }
            if (config.sourceOption == selectedSourceOption) {
                AutomationError error;
                error.code = AutomationErrorCode::Unsupported;
                error.fieldPath = QStringLiteral("config.source_option");
                error.message = QStringLiteral("Selected-track audio export is unavailable");
                return error;
            }
            if (config.timeRange < 0 || config.timeRange > 1) {
                return AutomationError::invalidArgument(
                    QStringLiteral("config.time_range"),
                    QStringLiteral("Audio export time range is invalid"));
            }
            if (config.timeRange == loopSectionRange) {
                AutomationError error;
                error.code = AutomationErrorCode::Unsupported;
                error.fieldPath = QStringLiteral("config.time_range");
                error.message = QStringLiteral("Loop-section audio export is unavailable");
                return error;
            }
            QSet<int> uniqueSources;
            for (const auto source : config.sources) {
                if (source < 0) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("config.sources"),
                        QStringLiteral("Audio export source indices must be non-negative"));
                }
                if (uniqueSources.contains(source)) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("config.sources"),
                        QStringLiteral("Audio export source indices must be unique"));
                }
                uniqueSources.insert(source);
            }
            return AutomationUnit{};
        }

        AutomationResult<AutomationUnit> validatePreview(const AudioExportPreviewDto &preview,
                                                         const AudioExportPolicyDto &policy) {
            if ((preview.warningFlags & AudioExportNoFile) && !policy.allowNoFiles) {
                return AutomationError::invalidArgument(
                    QStringLiteral("policy.allow_no_files"),
                    QStringLiteral("Audio export would not create any files"));
            }
            if ((preview.warningFlags & AudioExportDuplicatedFile) && !policy.allowDuplicatePaths) {
                return AutomationError::invalidArgument(
                    QStringLiteral("policy.allow_duplicate_paths"),
                    QStringLiteral("Audio export contains duplicate target paths"));
            }
            if ((preview.warningFlags & AudioExportWillOverwrite) && !policy.allowOverwrite) {
                AutomationError error;
                error.code = AutomationErrorCode::OverwriteDenied;
                error.fieldPath = QStringLiteral("policy.allow_overwrite");
                error.message = QStringLiteral("Audio export would overwrite existing files");
                return error;
            }
            if ((preview.warningFlags & AudioExportUnrecognizedTemplate) &&
                !policy.allowUnrecognizedTemplate) {
                return AutomationError::invalidArgument(
                    QStringLiteral("policy.allow_unrecognized_template"),
                    QStringLiteral("Audio export file-name template is not recognized"));
            }
            if ((preview.warningFlags & AudioExportLossyFormat) && !policy.allowLossyFormat) {
                return AutomationError::invalidArgument(
                    QStringLiteral("policy.allow_lossy_format"),
                    QStringLiteral("Audio export uses a lossy file format"));
            }
            const QDir baseDirectory(preview.baseDirectory);
            if (!baseDirectory.isAbsolute()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("config.file_directory"),
                    QStringLiteral("Audio export directory must resolve to an absolute path"));
            }
            if (!baseDirectory.exists()) {
                AutomationError error;
                error.code = AutomationErrorCode::FileNotFound;
                error.fieldPath = QStringLiteral("config.file_directory");
                error.message = QStringLiteral("Audio export target directory does not exist");
                return error;
            }
            for (const auto &path : preview.filePaths) {
                const QFileInfo fileInfo(path);
                if (!fileInfo.isAbsolute()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("config.file_directory"),
                        QStringLiteral("Audio export target paths must resolve to absolute paths"));
                }
                const auto relativePath =
                    baseDirectory.relativeFilePath(fileInfo.absoluteFilePath());
                if (QDir::isAbsolutePath(relativePath) || relativePath == QStringLiteral("..") ||
                    relativePath.startsWith(QStringLiteral("../")) ||
                    relativePath.startsWith(QStringLiteral("..\\"))) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("config.file_name"),
                        QStringLiteral("Audio export target escapes the configured directory"));
                }
                if (!fileInfo.dir().exists()) {
                    AutomationError error;
                    error.code = AutomationErrorCode::FileNotFound;
                    error.fieldPath = QStringLiteral("config.file_directory");
                    error.message = QStringLiteral("Audio export target directory does not exist");
                    return error;
                }
                if (fileInfo.exists() && !fileInfo.isFile()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("config.file_directory"),
                        QStringLiteral("Audio export target is not a file"));
                }
            }
            return AutomationUnit{};
        }

        bool terminal(const AutomationTaskState state) {
            return state == AutomationTaskState::Succeeded ||
                   state == AutomationTaskState::Failed || state == AutomationTaskState::Canceled;
        }
    }

    struct AudioExportAutomationFacade::PendingJobState {
        QMutex mutex;
        bool cancellationRequested = false;
        bool cleanupClaimed = false;
        std::shared_ptr<IAudioExportJob> job;

        void requestCancel() {
            std::shared_ptr<IAudioExportJob> current;
            {
                const QMutexLocker locker(&mutex);
                const auto firstRequest = !cancellationRequested;
                cancellationRequested = true;
                if (firstRequest)
                    current = job;
            }
            if (current)
                current->cancel();
        }

        std::shared_ptr<IAudioExportJob> claimCleanup() {
            const QMutexLocker locker(&mutex);
            if (cleanupClaimed || !job)
                return {};
            cleanupClaimed = true;
            return job;
        }
    };

    struct AudioExportAutomationFacade::JobRecord {
        DocumentVersion baseDocument;
        std::shared_ptr<PendingJobState> state;
    };

    AudioExportAutomationFacade::AudioExportAutomationFacade(
        AutomationDispatcher &dispatcher, IDocumentSessionResolver &documentResolver,
        AutomationTaskManager &tasks, AudioExportRuntimeServices services)
        : m_dispatcher(dispatcher), m_documentResolver(documentResolver), m_tasks(tasks),
          m_services(std::move(services)) {
    }

    AudioExportCapabilitiesDto AudioExportAutomationFacade::capabilities() {
        return {
            .formats = {QStringLiteral("wav"), QStringLiteral("flac"), QStringLiteral("ogg"),
                        QStringLiteral("mp3")},
            .sampleRates = {44100, 48000},
            .channelModes = {QStringLiteral("mono"), QStringLiteral("stereo")},
            .mixingModes = {QStringLiteral("mixed"), QStringLiteral("separated"),
                        QStringLiteral("separated_through_master")},
            .sourceModes = {QStringLiteral("all"), QStringLiteral("custom")},
        };
    }

    AutomationResult<AudioExportPreviewDto>
        AudioExportAutomationFacade::preview(const DocumentId &documentId,
                                             const AudioExportConfigDto &config) {
        return m_dispatcher.dispatchDocumentQuery<AudioExportPreviewDto>(
            OperationIds::exports::audio::preview, documentId,
            [this, config](DocumentSession &session) {
                const auto valid = validateConfig(config);
                if (!valid)
                    return AutomationResult<AudioExportPreviewDto>(valid.getError());
                if (!m_services.createJob)
                    return AutomationResult<AudioExportPreviewDto>(unavailable());
                auto job = m_services.createJob(session.model(), session.path(), config);
                if (!job)
                    return AutomationResult<AudioExportPreviewDto>(job.getError());
                return AutomationResult<AudioExportPreviewDto>(job.get()->preview());
            });
    }

    AutomationResult<TaskAcceptedResult> AudioExportAutomationFacade::start(
        const CommandContext &context, const AudioExportConfigDto &config,
        const AudioExportPolicyDto &policy, AudioExportObserver observer) {
        return start(context, config, policy, std::move(observer), {}, {});
    }

    AutomationResult<TaskAcceptedResult> AudioExportAutomationFacade::start(
        const CommandContext &context, const AudioExportConfigDto &config,
        const AudioExportPolicyDto &policy, AudioExportObserver observer,
        AudioExportAccessRevalidator reauthorize) {
        return start(context, config, policy, std::move(observer), {}, std::move(reauthorize));
    }

    AutomationResult<TaskAcceptedResult> AudioExportAutomationFacade::start(
        const CommandContext &context, const AudioExportConfigDto &config,
        const AudioExportPolicyDto &policy, AudioExportObserver observer,
        AudioExportOutputAuthorizer authorizeOutputs, AudioExportAccessRevalidator reauthorize) {
        return m_dispatcher.dispatchDocumentCommandResult<TaskAcceptedResult>(
            OperationIds::exports::audio::start, context,
            [this, context, config, policy, observer = std::move(observer),
             authorizeOutputs = std::move(authorizeOutputs), reauthorize = std::move(reauthorize)](
                DocumentSession &session, const bool validateOnly) mutable {
                const auto valid = validateConfig(config);
                if (!valid)
                    return AutomationResult<TaskAcceptedResult>(valid.getError());
                if (!m_services.createJob)
                    return AutomationResult<TaskAcceptedResult>(unavailable());
                auto validationJob = m_services.createJob(session.model(), session.path(), config);
                if (!validationJob)
                    return AutomationResult<TaskAcceptedResult>(validationJob.getError());
                const auto preview = validationJob.get()->preview();
                const auto validPreview = validatePreview(preview, policy);
                if (!validPreview)
                    return AutomationResult<TaskAcceptedResult>(validPreview.getError());
                if (authorizeOutputs) {
                    auto authorized = authorizeOutputs(preview);
                    if (!authorized)
                        return AutomationResult<TaskAcceptedResult>(authorized.getError());
                }
                if (validateOnly) {
                    return AutomationResult<TaskAcceptedResult>({
                        .document = session.version(),
                        .validatedOnly = true,
                    });
                }

                auto state = std::make_shared<PendingJobState>();
                {
                    const QMutexLocker locker(&state->mutex);
                    state->job = validationJob.get();
                }
                const auto task = m_tasks.createTask(
                    OperationIds::exports::audio::start, session.version(), std::nullopt,
                    [weak = std::weak_ptr<PendingJobState>(state)] {
                        if (const auto locked = weak.lock())
                            locked->requestCancel();
                    },
                    context.clientId);
                auto record = std::make_shared<JobRecord>();
                record->baseDocument = session.version();
                record->state = state;
                {
                    const QMutexLocker locker(&m_jobsMutex);
                    m_jobs.insert(task.taskId, record);
                }

                const TaskAcceptedResult accepted{
                    .taskId = task.taskId,
                    .document = session.version(),
                };

                auto execute = [this, taskId = task.taskId, base = session.version(),
                                observer = std::move(observer),
                                reauthorize = std::move(reauthorize), state,
                                allowOverwrite = policy.allowOverwrite]() mutable {
                    executeTask(taskId, base, std::move(observer), std::move(reauthorize), state,
                                allowOverwrite);
                };
                if (m_services.schedule)
                    m_services.schedule(execute);
                else
                    execute();
                return AutomationResult<TaskAcceptedResult>(accepted);
            });
    }

    AutomationResult<ApplicationMutationResult>
        AudioExportAutomationFacade::cleanup(const CommandContext &context, const TaskId &taskId) {
        return m_dispatcher.dispatchDocumentCommandResult<ApplicationMutationResult>(
            OperationIds::exports::audio::cleanup, context,
            [this, taskId](DocumentSession &session, const bool validateOnly) {
                const auto task = m_tasks.get(session.documentId(), taskId);
                if (!task)
                    return AutomationResult<ApplicationMutationResult>(task.getError());
                if (!terminal(task.get().state)) {
                    AutomationError error;
                    error.code = AutomationErrorCode::Busy;
                    error.taskId = taskId;
                    error.message = QStringLiteral("Audio export is still running");
                    return AutomationResult<ApplicationMutationResult>(std::move(error));
                }
                std::shared_ptr<JobRecord> record;
                {
                    const QMutexLocker locker(&m_jobsMutex);
                    record = m_jobs.value(taskId);
                }
                if (!record) {
                    return AutomationResult<ApplicationMutationResult>({
                        .validatedOnly = validateOnly,
                    });
                }
                if (!validateOnly) {
                    const auto job = record->state->claimCleanup();
                    if (job)
                        job->cleanup();
                    removeJobRecord(taskId);
                }
                return AutomationResult<ApplicationMutationResult>({
                    .changed = true,
                    .validatedOnly = validateOnly,
                });
            });
    }

    void AudioExportAutomationFacade::executeTask(const TaskId &taskId,
                                                  const DocumentVersion baseDocument,
                                                  AudioExportObserver observer,
                                                  AudioExportAccessRevalidator reauthorize,
                                                  const std::shared_ptr<PendingJobState> &state,
                                                  const bool allowOverwrite) {
        std::shared_ptr<IAudioExportJob> job;
        bool cancelBeforeRun = false;
        {
            const QMutexLocker locker(&state->mutex);
            job = state->job;
            cancelBeforeRun = state->cancellationRequested;
        }
        if (!job) {
            m_tasks.fail(taskId, taskError(unavailable(), taskId));
            removeJobRecord(taskId);
            return;
        }
        const auto cleanup = [state] {
            if (const auto current = state->claimCleanup())
                current->cleanup();
        };
        if (cancelBeforeRun || m_tasks.isCancellationRequested(taskId)) {
            if (!cancelBeforeRun)
                job->cancel();
            m_tasks.cancel(taskId);
            cleanup();
            removeJobRecord(taskId);
            return;
        }
        auto resolved = resolveDocumentGeneration(baseDocument);
        if (!resolved) {
            m_tasks.fail(taskId, taskError(resolved.getError(), taskId));
            cleanup();
            removeJobRecord(taskId);
            return;
        }
        if (!m_tasks.markRunning(taskId)) {
            cleanup();
            removeJobRecord(taskId);
            return;
        }
        if (reauthorize) {
            auto authorized = reauthorize();
            if (!authorized) {
                m_tasks.fail(taskId, taskError(authorized.getError(), taskId));
                cleanup();
                removeJobRecord(taskId);
                return;
            }
        }

        QStringList warnings;
        AudioExportObserver taskObserver;
        taskObserver.progress = [this, taskId, callback = std::move(observer.progress)](
                                    const double progress, const int sourceIndex) {
            m_tasks.updateProgress(taskId,
                                   {.minimum = 0,
                                    .maximum = 100,
                                    .value = std::clamp(static_cast<int>(progress * 100.0), 0, 100),
                                    .indeterminate = false});
            if (callback)
                callback(progress, sourceIndex);
        };
        taskObserver.inferenceProgress = [this, taskId,
                                          callback = std::move(observer.inferenceProgress)](
                                             const double progress) {
            m_tasks.updateProgress(taskId,
                                   {.minimum = 0,
                                    .maximum = 100,
                                    .value = std::clamp(static_cast<int>(progress * 100.0), 0, 100),
                                    .indeterminate = true});
            if (callback)
                callback(progress);
        };
        taskObserver.clipping = [&warnings,
                                 callback = std::move(observer.clipping)](const int sourceIndex) {
            warnings.append(
                sourceIndex < 0
                    ? QStringLiteral("Clipping detected")
                    : QStringLiteral("Clipping detected in source %1").arg(sourceIndex));
            if (callback)
                callback(sourceIndex);
        };
        taskObserver.warning = [&warnings, callback = std::move(observer.warning)](
                                   const QString &message, const int sourceIndex) {
            warnings.append(message);
            if (callback)
                callback(message, sourceIndex);
        };

        const auto readiness = job->waitUntilReady(taskObserver);
        if (readiness.state == AudioExportBackendState::Canceled ||
            m_tasks.isCancellationRequested(taskId)) {
            m_tasks.cancel(taskId);
            cleanup();
            removeJobRecord(taskId);
            return;
        }
        if (readiness.state == AudioExportBackendState::Failed) {
            AutomationError error;
            error.code = AutomationErrorCode::IoError;
            error.taskId = taskId;
            error.message = readiness.errorMessage.isEmpty()
                                ? QStringLiteral("Audio export preparation failed")
                                : readiness.errorMessage;
            m_tasks.fail(taskId, std::move(error));
            cleanup();
            removeJobRecord(taskId);
            return;
        }
        resolved = resolveDocumentGeneration(baseDocument);
        if (!resolved) {
            m_tasks.fail(taskId, taskError(resolved.getError(), taskId));
            cleanup();
            removeJobRecord(taskId);
            return;
        }
        if (reauthorize) {
            auto authorized = reauthorize();
            if (!authorized) {
                m_tasks.fail(taskId, taskError(authorized.getError(), taskId));
                cleanup();
                removeJobRecord(taskId);
                return;
            }
        }

        const auto deferPublish = static_cast<bool>(reauthorize) || !allowOverwrite;
        const auto result = job->execute(taskObserver, deferPublish);
        if (result.state == AudioExportBackendState::Canceled ||
            m_tasks.isCancellationRequested(taskId)) {
            m_tasks.cancel(taskId);
            cleanup();
            removeJobRecord(taskId);
            return;
        }
        if (result.state == AudioExportBackendState::Failed) {
            AutomationError error;
            error.code = AutomationErrorCode::IoError;
            error.taskId = taskId;
            error.message = result.errorMessage.isEmpty() ? QStringLiteral("Audio export failed")
                                                          : result.errorMessage;
            m_tasks.fail(taskId, std::move(error));
            cleanup();
            removeJobRecord(taskId);
            return;
        }
        resolved = resolveDocumentGeneration(baseDocument);
        if (!resolved) {
            m_tasks.fail(taskId, taskError(resolved.getError(), taskId));
            cleanup();
            removeJobRecord(taskId);
            return;
        }
        if (reauthorize) {
            auto authorized = reauthorize();
            if (!authorized) {
                m_tasks.fail(taskId, taskError(authorized.getError(), taskId));
                cleanup();
                removeJobRecord(taskId);
                return;
            }
        }
        const auto currentDocument = resolved.get().get().version();
        const auto committing = m_tasks.beginCommitting(taskId);
        if (!committing || !committing.get()) {
            cleanup();
            removeJobRecord(taskId);
            return;
        }
        if (deferPublish) {
            const auto published = job->publish(allowOverwrite);
            if (published.state != AudioExportBackendState::Succeeded) {
                AutomationError error;
                error.code = AutomationErrorCode::IoError;
                error.taskId = taskId;
                error.message = published.errorMessage.isEmpty()
                                    ? QStringLiteral("Audio export publication failed")
                                    : published.errorMessage;
                m_tasks.fail(taskId, std::move(error));
                cleanup();
                removeJobRecord(taskId);
                return;
            }
        }
        MutationResult mutation;
        mutation.previous = currentDocument;
        mutation.current = currentDocument;
        mutation.warnings = std::move(warnings);
        if (!m_tasks.succeed(taskId, std::move(mutation)))
            cleanup();
        removeJobRecord(taskId);
    }

    void AudioExportAutomationFacade::removeJobRecord(const TaskId &taskId) {
        const QMutexLocker locker(&m_jobsMutex);
        m_jobs.remove(taskId);
    }

    AutomationResult<std::reference_wrapper<DocumentSession>>
        AudioExportAutomationFacade::resolveDocumentGeneration(
            const DocumentVersion &version) const {
        auto resolved = m_documentResolver.resolveDocument(version.documentId);
        if (!resolved)
            return resolved.getError();
        auto &session = resolved.get().get();
        if (session.lifecycleState() != DocumentLifecycleState::Active)
            return AutomationError::documentBusy(session.documentId());
        const auto rebased = rebaseTaskVersionWithinGeneration(version, session.version());
        if (!rebased)
            return rebased.getError();
        return std::ref(session);
    }

    void AudioExportAutomationFacade::discardDocumentGeneration(const DocumentId &documentId) {
        QList<std::shared_ptr<JobRecord>> removed;
        {
            const QMutexLocker locker(&m_jobsMutex);
            for (auto it = m_jobs.begin(); it != m_jobs.end();) {
                const auto record = it.value();
                if (!record || !record->state) {
                    it = m_jobs.erase(it);
                    continue;
                }
                if (record->baseDocument.documentId != documentId) {
                    ++it;
                    continue;
                }
                removed.append(record);
                it = m_jobs.erase(it);
            }
        }
        for (const auto &record : std::as_const(removed))
            record->state->requestCancel();
    }

} // namespace Automation
