#include "AudioExportAutomationFacade.h"
#include "OperationIds.h"

#include <QDataStream>
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

        AutomationError unsupported(const QString &field, const QString &message) {
            AutomationError error;
            error.code = AutomationErrorCode::Unsupported;
            error.fieldPath = field;
            error.message = message;
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
                return unsupported(
                    QStringLiteral("config.source_option"),
                    QStringLiteral("Selected-track audio export is not implemented"));
            }
            if (config.timeRange < 0 || config.timeRange > 1) {
                return AutomationError::invalidArgument(
                    QStringLiteral("config.time_range"),
                    QStringLiteral("Audio export time range is invalid"));
            }
            if (config.timeRange == loopSectionRange) {
                return unsupported(QStringLiteral("config.time_range"),
                                   QStringLiteral("Loop-section audio export is not implemented"));
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

        QByteArray fingerprint(const AudioExportConfigDto &config,
                               const AudioExportPolicyDto &policy) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            const auto formatOption = config.fileType >= 2 ? 0 : config.formatOption;
            const auto sources = config.sourceOption == 0 ? QList<int>() : config.sources;
            stream << config.fileName << config.fileDirectory << config.fileType << config.mono
                   << formatOption << config.formatQuality << config.sampleRate
                   << config.mixingOption << config.muteSoloEnabled << config.sourceOption
                   << sources << config.timeRange << policy.allowNoFiles
                   << policy.allowDuplicatePaths << policy.allowOverwrite
                   << policy.allowUnrecognizedTemplate << policy.allowLossyFormat;
            return result;
        }

        bool terminal(const AutomationTaskState state) {
            return state == AutomationTaskState::Succeeded ||
                   state == AutomationTaskState::Failed || state == AutomationTaskState::Canceled;
        }
    }

    struct AudioExportAutomationFacade::PendingJobState {
        QMutex mutex;
        bool cancellationRequested = false;
        std::shared_ptr<IAudioExportJob> job;

        void requestCancel() {
            std::shared_ptr<IAudioExportJob> current;
            {
                const QMutexLocker locker(&mutex);
                cancellationRequested = true;
                current = job;
            }
            if (current)
                current->cancel();
        }
    };

    struct AudioExportAutomationFacade::JobRecord {
        DocumentVersion baseDocument;
        std::shared_ptr<PendingJobState> state;
    };

    AudioExportAutomationFacade::AudioExportAutomationFacade(
        OperationCatalog &catalog, AutomationDispatcher &dispatcher,
        IDocumentSessionResolver &documentResolver, AutomationTaskManager &tasks,
        AudioExportRuntimeServices services)
        : m_catalog(catalog), m_dispatcher(dispatcher), m_documentResolver(documentResolver),
          m_tasks(tasks), m_services(std::move(services)) {
        registerOperations();
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
        const auto requestFingerprint = fingerprint(config, policy);
        return m_dispatcher.dispatchDocumentCommandResult<TaskAcceptedResult>(
            OperationIds::exports::audio::start, context, requestFingerprint,
            [this, context, requestFingerprint, config, policy, observer = std::move(observer)](
                DocumentSession &session, const bool validateOnly) mutable {
                const auto valid = validateConfig(config);
                if (!valid)
                    return AutomationResult<TaskAcceptedResult>(valid.getError());
                if (!m_services.createJob)
                    return AutomationResult<TaskAcceptedResult>(unavailable());
                auto validationJob = m_services.createJob(session.model(), session.path(), config);
                if (!validationJob)
                    return AutomationResult<TaskAcceptedResult>(validationJob.getError());
                const auto validPreview = validatePreview(validationJob.get()->preview(), policy);
                if (!validPreview)
                    return AutomationResult<TaskAcceptedResult>(validPreview.getError());
                if (validateOnly) {
                    return AutomationResult<TaskAcceptedResult>({
                        .document = session.version(),
                        .validatedOnly = true,
                    });
                }

                auto state = std::make_shared<PendingJobState>();
                const auto task = m_tasks.createTask(
                    OperationIds::exports::audio::start, session.version(), std::nullopt,
                    [weak = std::weak_ptr<PendingJobState>(state)] {
                        if (const auto locked = weak.lock())
                            locked->requestCancel();
                    });
                auto record = std::make_shared<JobRecord>();
                record->baseDocument = session.version();
                record->state = state;
                m_jobs.insert(task.taskId, record);

                const TaskAcceptedResult accepted{
                    .taskId = task.taskId,
                    .document = session.version(),
                };
                if (!context.idempotencyKey.isEmpty()) {
                    const auto bound = m_tasks.setUnsuccessfulCallback(
                        task.taskId, [this, context, requestFingerprint,
                                      accepted](const AutomationTaskSnapshot &) {
                            m_dispatcher.releaseDocumentIdempotency(
                                OperationIds::exports::audio::start, context, requestFingerprint,
                                accepted);
                        });
                    Q_ASSERT(bound);
                }

                auto execute = [this, taskId = task.taskId, base = session.version(), config,
                                observer = std::move(observer), state]() mutable {
                    executeTask(taskId, base, std::move(config), std::move(observer), state);
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
            OperationIds::exports::audio::cleanup, context, taskId.toString().toUtf8(),
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
                const auto found = m_jobs.constFind(taskId);
                if (found == m_jobs.cend()) {
                    return AutomationResult<ApplicationMutationResult>({
                        .validatedOnly = validateOnly,
                    });
                }
                if (!validateOnly) {
                    std::shared_ptr<IAudioExportJob> job;
                    {
                        const QMutexLocker locker(&(*found)->state->mutex);
                        job = (*found)->state->job;
                    }
                    if (job)
                        job->cleanup();
                    m_jobs.remove(taskId);
                }
                return AutomationResult<ApplicationMutationResult>({
                    .changed = true,
                    .validatedOnly = validateOnly,
                });
            });
    }

    void AudioExportAutomationFacade::executeTask(const TaskId &taskId,
                                                  const DocumentVersion baseDocument,
                                                  AudioExportConfigDto config,
                                                  AudioExportObserver observer,
                                                  const std::shared_ptr<PendingJobState> &state) {
        if (m_tasks.isCancellationRequested(taskId)) {
            m_tasks.cancel(taskId);
            return;
        }
        auto resolved = resolveVersion(baseDocument);
        if (!resolved) {
            m_tasks.fail(taskId, taskError(resolved.getError(), taskId));
            return;
        }
        if (!m_services.createJob) {
            m_tasks.fail(taskId, taskError(unavailable(), taskId));
            return;
        }
        auto created =
            m_services.createJob(resolved.get().get().model(), resolved.get().get().path(), config);
        if (!created) {
            m_tasks.fail(taskId, taskError(created.getError(), taskId));
            return;
        }
        bool cancelBeforeRun = false;
        {
            const QMutexLocker locker(&state->mutex);
            state->job = created.get();
            cancelBeforeRun = state->cancellationRequested;
        }
        if (cancelBeforeRun || m_tasks.isCancellationRequested(taskId)) {
            created.get()->cancel();
            m_tasks.cancel(taskId);
            return;
        }
        if (!m_tasks.markRunning(taskId))
            return;

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

        const auto result = created.get()->execute(taskObserver);
        if (result.state == AudioExportBackendState::Canceled ||
            m_tasks.isCancellationRequested(taskId)) {
            m_tasks.cancel(taskId);
            return;
        }
        if (result.state == AudioExportBackendState::Failed) {
            AutomationError error;
            error.code = AutomationErrorCode::IoError;
            error.taskId = taskId;
            error.message = result.errorMessage.isEmpty() ? QStringLiteral("Audio export failed")
                                                          : result.errorMessage;
            m_tasks.fail(taskId, std::move(error));
            return;
        }

        resolved = resolveVersion(baseDocument);
        if (!resolved) {
            created.get()->cleanup();
            m_tasks.fail(taskId, taskError(resolved.getError(), taskId));
            return;
        }
        const auto committing = m_tasks.beginCommitting(taskId);
        if (!committing || !committing.get()) {
            created.get()->cleanup();
            return;
        }
        MutationResult mutation;
        mutation.previous = baseDocument;
        mutation.current = baseDocument;
        mutation.warnings = std::move(warnings);
        m_tasks.succeed(taskId, std::move(mutation));
    }

    AutomationResult<std::reference_wrapper<DocumentSession>>
        AudioExportAutomationFacade::resolveVersion(const DocumentVersion &version) const {
        auto resolved = m_documentResolver.resolveDocument(version.documentId);
        if (!resolved)
            return resolved.getError();
        auto &session = resolved.get().get();
        if (session.lifecycleState() != DocumentLifecycleState::Active)
            return AutomationError::documentBusy(session.documentId());
        if (session.revision() != version.revision) {
            return AutomationError::revisionConflict(session.documentId(), version.revision,
                                                     session.revision());
        }
        return std::ref(session);
    }

    void AudioExportAutomationFacade::discardDocumentGeneration(const DocumentId &documentId) {
        for (auto it = m_jobs.begin(); it != m_jobs.end();) {
            if ((*it)->baseDocument.documentId != documentId) {
                ++it;
                continue;
            }
            (*it)->state->requestCancel();
            std::shared_ptr<IAudioExportJob> job;
            {
                const QMutexLocker locker(&(*it)->state->mutex);
                job = (*it)->state->job;
            }
            const auto task = m_tasks.get(documentId, it.key());
            if (job && task && terminal(task.get().state))
                job->cleanup();
            it = m_jobs.erase(it);
        }
    }

    void AudioExportAutomationFacade::registerOperations() {
        const auto add = [this](OperationDescriptor descriptor) {
            const auto result = m_catalog.add(std::move(descriptor));
            Q_ASSERT(result);
        };
        add({
            .id = OperationIds::exports::audio::preview,
            .category = QStringLiteral("exports"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .documentPolicy = DocumentPolicy::Read,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::Read,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
        add({
            .id = OperationIds::exports::audio::start,
            .category = QStringLiteral("exports"),
            .kind = OperationKind::Command,
            .syncMode = SyncMode::Asynchronous,
            .documentPolicy = DocumentPolicy::Read,
            .revisionPolicy = RevisionPolicy::Check,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::Write,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::FileSystem,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::DocumentGeneration,
        });
        add({
            .id = OperationIds::exports::audio::cleanup,
            .category = QStringLiteral("exports"),
            .kind = OperationKind::Command,
            .syncMode = SyncMode::Synchronous,
            .documentPolicy = DocumentPolicy::Read,
            .revisionPolicy = RevisionPolicy::Check,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::Write,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::FileSystem,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
    }

} // namespace Automation
