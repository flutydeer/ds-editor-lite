#include "PublicAutomationHostAdapter.h"

#include "../CoreRuntime.h"
#include "../OperationIds.h"
#include "Controller/DocumentWorkflow/IProjectLoadSession.h"
#include "Controller/Tasks/ComputeAudioHashTask.h"
#include "Controller/Tasks/DecodeAudioTask.h"
#include "Modules/Import/AudioFilePreparer.h"
#include "Modules/ProjectFormats/IProjectFormatHandler.h"
#include "Modules/ProjectFormats/ProjectFormatRegistry.h"
#include "Modules/Inference/InferController.h"
#include "Modules/Inference/InferEngine.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/ProjectModel/Utils/DiffscopeAudioWorkspace.h>
#include <lite/AutomationWire/JsonSchema.h>
#include <lite/AutomationWire/PublicToolContract.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QSet>
#include <QThreadPool>
#include <QTimer>

#include <algorithm>
#include <limits>
#include <memory>
#include <variant>

namespace Automation {
    namespace {
        AutomationError unavailable(QString message) {
            AutomationError result;
            result.code = AutomationErrorCode::HostCapabilityUnavailable;
            result.message = std::move(message);
            return result;
        }

        AutomationError audioPathPreparationError(QString message) {
            AutomationError result;
            result.code = AutomationErrorCode::IoError;
            result.fieldPath = QStringLiteral("path");
            result.message = std::move(message);
            return result;
        }

        AutomationResult<PublicPreparedAudioPath> prepareAudioPath(const QString &path) {
            std::unique_ptr<DecodeAudioTask> decodeTask(
                AudioFilePreparer::createPrepareTask(path));
            if (!decodeTask || !decodeTask->io) {
                return audioPathPreparationError(
                    QStringLiteral("No audio decoder is available for the selected file"));
            }
            static_cast<QRunnable *>(decodeTask.get())->run();
            if (!decodeTask->success || decodeTask->terminated()) {
                return audioPathPreparationError(QStringLiteral("Audio decoding failed"));
            }

            auto hashTask = std::make_unique<ComputeAudioHashTask>();
            hashTask->path = path;
            static_cast<QRunnable *>(hashTask.get())->run();
            if (!hashTask->success || hashTask->resultSha512.isEmpty()) {
                return audioPathPreparationError(QStringLiteral("Failed to compute audio hash"));
            }
            return PublicPreparedAudioPath{hashTask->resultSha512, decodeTask->workspace};
        }

        AutomationResult<DocumentVersion> validateBase(CoreRuntime &runtime,
                                                       const CommandContext &context) {
            auto document = runtime.documents().getDocument(context.expected.documentId);
            if (!document)
                return document.getError();
            if (document.get().document.revision != context.expected.revision) {
                return AutomationError::revisionConflict(context.expected.documentId,
                                                         context.expected.revision,
                                                         document.get().document.revision);
            }
            return document.get().document;
        }

        AutomationError projectLoadError(const ProjectOperationError &source,
                                         const TaskId &taskId) {
            AutomationError result;
            result.code = AutomationErrorCode::IoError;
            result.message = source.message.isEmpty() ? source.title : source.message;
            result.taskId = taskId;
            return result;
        }

        class HeadlessProjectLoadTask final : public QObject {
        public:
            HeadlessProjectLoadTask(CoreRuntime &runtime, QString path, QString mergeMode,
                                    const ProjectLoadPurpose purpose, const bool importTempo,
                                    const bool importTimeSignature, CommandContext command,
                                    QObject *parent)
                : QObject(parent), m_runtime(runtime), m_path(std::move(path)),
                  m_mergeMode(std::move(mergeMode)), m_purpose(purpose),
                  m_importTempo(importTempo), m_importTimeSignature(importTimeSignature),
                  m_command(std::move(command)) {
            }

            AutomationResult<TaskAcceptedResult> prepare() {
                auto base = validateBase(m_runtime, m_command);
                if (!base)
                    return base.getError();
                auto *handler = projectFormatRegistry->resolveByPath(m_path);
                if (!handler) {
                    AutomationError error;
                    error.code = AutomationErrorCode::FormatUnsupported;
                    error.fieldPath = QStringLiteral("path");
                    error.message = QStringLiteral("No project format handler accepts the file");
                    return error;
                }
                const auto descriptor = handler->descriptor();
                const bool supported = m_purpose == ProjectLoadPurpose::Open
                                           ? descriptor.canOpen
                                           : descriptor.canImport;
                if (!supported) {
                    AutomationError error;
                    error.code = AutomationErrorCode::FormatUnsupported;
                    error.fieldPath = QStringLiteral("path");
                    error.message = QStringLiteral("The project format does not support this operation");
                    return error;
                }
                if (m_command.validateOnly)
                    return TaskAcceptedResult{{}, base.get(), true};

                const auto operation = m_purpose == ProjectLoadPurpose::Open
                                           ? QStringLiteral("documents.open")
                                           : QStringLiteral("documents.import");
                const QPointer<HeadlessProjectLoadTask> weak(this);
                const auto task = m_runtime.automationTasks().createTask(
                    operation, base.get(), std::nullopt,
                    [weak] {
                        if (weak)
                            weak->cancel();
                    },
                    m_command.clientId);
                m_taskId = task.taskId;
                m_command.taskId = m_taskId;
                ProjectLoadRequest request{
                    .filePath = m_path,
                    .purpose = m_purpose,
                    .requestId = 1,
                    .interactive = false,
                    .importTempo = m_importTempo,
                    .importTimeSignature = m_importTimeSignature,
                };
                m_session = handler->createSession(request, nullptr, this);
                if (!m_session) {
                    auto error = unavailable(QStringLiteral("The project format could not create a load session"));
                    error.taskId = m_taskId;
                    m_runtime.automationTasks().fail(m_taskId, error);
                    return error;
                }
                connect(m_session, &IProjectLoadSession::ready, this,
                        [this] { commit(m_session->takeResult()); });
                connect(m_session, &IProjectLoadSession::failed, this,
                        [this](const ProjectOperationError &error) {
                            m_runtime.automationTasks().fail(m_taskId,
                                                             projectLoadError(error, m_taskId));
                            deleteLater();
                        });
                connect(m_session, &IProjectLoadSession::canceled, this, [this] {
                    m_runtime.automationTasks().cancel(m_taskId);
                    deleteLater();
                });
                if (!m_runtime.automationTasks().markRunning(m_taskId)) {
                    auto error = unavailable(QStringLiteral("The project load task could not start"));
                    error.taskId = m_taskId;
                    return error;
                }
                QTimer::singleShot(0, m_session, [session = QPointer(m_session)] {
                    if (session)
                        session->start();
                });
                return TaskAcceptedResult{m_taskId, base.get(), false};
            }

        private:
            void cancel() {
                if (m_session)
                    m_session->cancel();
            }

            void commit(PreparedProject prepared) {
                const auto committing = m_runtime.automationTasks().beginCommitting(m_taskId);
                if (!committing || !committing.get()) {
                    if (!committing)
                        m_runtime.automationTasks().fail(m_taskId, committing.getError());
                    deleteLater();
                    return;
                }
                AutomationResult<MutationResult> result = unavailable(
                    QStringLiteral("The project loader returned no project"));
                if (auto *replace = std::get_if<ReplaceProjectPayload>(&prepared)) {
                    const auto draft = documentDraftDto(replace->model, replace->loopSettings);
                    result = m_runtime.documents().commitOpenedDocument(
                        m_command, draft,
                        replace->sourceKind == ProjectSourceKind::Native ? replace->sourcePath
                                                                        : QString(),
                        replace->sourceKind == ProjectSourceKind::Native
                            ? QFileInfo(replace->sourcePath).fileName()
                            : replace->displayName,
                        replace->sourceKind == ProjectSourceKind::Native);
                } else if (auto *append = std::get_if<AppendProjectPayload>(&prepared)) {
                    auto draft = documentDraftDto(append->model);
                    if (m_mergeMode == QStringLiteral("replace")) {
                        result = m_runtime.documents().commitOpenedDocument(
                            m_command, draft, {}, QFileInfo(m_path).fileName(), false);
                    } else {
                        result = m_runtime.documents().commitImportedDocument(
                            m_command, draft, m_importTempo && append->importTempo,
                            m_importTimeSignature && append->importTimeSignature);
                    }
                }
                if (result)
                    m_runtime.automationTasks().succeed(m_taskId, result.get());
                else
                    m_runtime.automationTasks().fail(m_taskId, result.getError());
                deleteLater();
            }

            CoreRuntime &m_runtime;
            QString m_path;
            QString m_mergeMode;
            ProjectLoadPurpose m_purpose;
            bool m_importTempo = true;
            bool m_importTimeSignature = true;
            CommandContext m_command;
            TaskId m_taskId;
            QPointer<IProjectLoadSession> m_session;
        };

        AutomationResult<TaskAcceptedResult> startProjectLoad(
            CoreRuntime &runtime, const QString &path, const QString &mergeMode,
            const ProjectLoadPurpose purpose, const bool importTempo,
            const bool importTimeSignature, CommandContext command) {
            auto *state = new HeadlessProjectLoadTask(
                runtime, path, mergeMode, purpose, importTempo, importTimeSignature,
                std::move(command), QCoreApplication::instance());
            auto result = state->prepare();
            if (!result || result.get().validatedOnly)
                state->deleteLater();
            return result;
        }

        ClipDraftDto audioClipDraft(const QString &path,
                                    const std::optional<PublicAudioClipProperties> &properties,
                                    const QString &clientRef) {
            ClipDraftDto clip;
            clip.type = ClipDraftDto::Type::Audio;
            clip.audioPath = path;
            clip.clientRef = clientRef;
            if (properties) {
                if (properties->name)
                    clip.properties.name = *properties->name;
                if (properties->start)
                    clip.properties.start = *properties->start;
                if (properties->length)
                    clip.properties.length = *properties->length;
                if (properties->clipStart)
                    clip.properties.clipStart = *properties->clipStart;
                if (properties->clipLength)
                    clip.properties.clipLen = *properties->clipLength;
                if (properties->gain)
                    clip.properties.gain = *properties->gain;
                if (properties->mute)
                    clip.properties.mute = *properties->mute;
            }
            return clip;
        }

        class HeadlessAudioImportTask final : public QObject {
        public:
            HeadlessAudioImportTask(CoreRuntime &runtime, AppModel *model, CommandContext command,
                                    QList<ClipInsertDto> clips,
                                    const PublicBatchFailurePolicy failurePolicy,
                                    QString operationId, QObject *parent)
                : QObject(parent), m_runtime(runtime), m_model(model), m_command(std::move(command)),
                  m_failurePolicy(failurePolicy), m_operationId(std::move(operationId)) {
                m_entries.reserve(clips.size());
                for (auto &clip : clips)
                    m_entries.append({std::move(clip)});
            }

            AutomationResult<TaskAcceptedResult> prepare() {
                auto base = validateBase(m_runtime, m_command);
                if (!base)
                    return base.getError();
                if (!m_model || m_entries.isEmpty()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("items"), QStringLiteral("No audio import items were supplied"));
                }

                for (auto &entry : m_entries) {
                    entry.task = AudioFilePreparer::createPrepareTask(entry.request.clip.audioPath);
                    if (!entry.task || !entry.task->io) {
                        const auto path = entry.request.clip.audioPath;
                        delete entry.task;
                        entry.task = nullptr;
                        entry.error = QStringLiteral("No audio decoder is available for %1").arg(path);
                    }
                }

                if (m_command.validateOnly) {
                    const auto invalid = std::find_if(
                        m_entries.cbegin(), m_entries.cend(),
                        [](const Entry &entry) { return !entry.error.isEmpty(); });
                    for (auto &entry : m_entries) {
                        delete entry.task;
                        entry.task = nullptr;
                    }
                    if (invalid != m_entries.cend() &&
                        m_failurePolicy == PublicBatchFailurePolicy::Atomic) {
                        return importError(invalid->error, invalid - m_entries.cbegin());
                    }
                    QList<ClipInsertDto> candidates;
                    for (const auto &entry : std::as_const(m_entries)) {
                        if (entry.error.isEmpty())
                            candidates.append(entry.request);
                    }
                    if (candidates.isEmpty()) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("path"),
                            QStringLiteral("No decodable audio import item is available"));
                    }
                    auto checked = m_runtime.project().insertClips(m_command, candidates);
                    if (!checked)
                        return checked.getError();
                    return TaskAcceptedResult{{}, base.get(), true};
                }

                const auto task = m_runtime.automationTasks().createTask(
                    m_operationId, base.get(), std::nullopt,
                    [guard = QPointer<HeadlessAudioImportTask>(this)] {
                        if (guard)
                            guard->requestCancel();
                    },
                    m_command.clientId);
                m_taskId = task.taskId;
                m_command.taskId = m_taskId;
                if (!m_runtime.automationTasks().markRunning(m_taskId)) {
                    return AutomationError::taskNotFound(m_taskId);
                }

                m_remaining = 0;
                for (auto &entry : m_entries) {
                    if (!entry.task)
                        continue;
                    ++m_remaining;
                    auto *decodeTask = entry.task;
                    connect(decodeTask, &Task::finished, this,
                            [this, decodeTask] { handleFinished(decodeTask); },
                            Qt::QueuedConnection);
                    QThreadPool::globalInstance()->start(decodeTask);
                }
                if (m_remaining == 0)
                    QTimer::singleShot(0, this, [this] { finishPreparation(); });
                return TaskAcceptedResult{m_taskId, base.get(), false};
            }

        private:
            struct Entry {
                ClipInsertDto request;
                DecodeAudioTask *task = nullptr;
                std::optional<PreparedAudioItem> prepared;
                QString error;
            };

            AutomationError importError(const QString &message, const qsizetype index) const {
                AutomationError result;
                result.code = AutomationErrorCode::IoError;
                result.operationId = m_operationId;
                result.taskId = m_taskId;
                result.fieldPath = m_entries.size() == 1
                                       ? QStringLiteral("path")
                                       : QStringLiteral("items[%1].path").arg(index);
                result.message = message;
                return result;
            }

            void requestCancel() {
                for (const auto &entry : std::as_const(m_entries)) {
                    if (entry.task)
                        entry.task->terminate();
                }
            }

            void handleFinished(DecodeAudioTask *task) {
                const auto found = std::find_if(m_entries.begin(), m_entries.end(),
                                                [task](const Entry &entry) {
                                                    return entry.task == task;
                                                });
                if (found == m_entries.end())
                    return;
                if (task->terminated()) {
                    found->error = QStringLiteral("Audio decoding was canceled");
                } else if (!task->success) {
                    found->error = task->errorMessage.isEmpty()
                                       ? QStringLiteral("Audio decoding failed")
                                       : task->errorMessage;
                } else {
                    found->prepared = AudioFilePreparer::prepareResult(task);
                }
                found->task = nullptr;
                task->deleteLater();
                if (--m_remaining == 0)
                    finishPreparation();
            }

            ClipInsertDto preparedInsert(const Entry &entry, const QString &projectPath) const {
                auto result = entry.request;
                const auto &prepared = *entry.prepared;
                auto &clip = result.clip;
                auto &properties = clip.properties;
                const auto &timeline = m_model->timeline();
                const auto start = properties.start;
                const auto startMs = timeline.tickToMs(start);
                const auto materialTicks =
                    std::max(1, qRound(timeline.msToTick(startMs + prepared.durationMs)) - start);
                if (properties.name.isEmpty())
                    properties.name = QFileInfo(prepared.path).baseName();
                if (properties.length <= 0)
                    properties.length = materialTicks;
                if (properties.clipLen <= 0)
                    properties.clipLen = std::max(1, properties.length - properties.clipStart);
                const auto visibleStart = start + properties.clipStart;
                properties.trimStartMs = timeline.tickToMs(visibleStart) - startMs;
                properties.playLengthMs =
                    timeline.tickToMs(visibleStart + properties.clipLen) -
                    timeline.tickToMs(visibleStart);
                properties.materialLengthMs = prepared.durationMs;
                clip.audioPath = prepared.path;
                clip.audioInfo = prepared.audioInfo;
                clip.audioPathInfo = {
                    DiffscopeAudioWorkspace::relativeDirFor(prepared.path, projectPath), {}};
                clip.hasRealTimeAnchor = true;
                clip.workspace.insert(QStringLiteral("diffscope.audio.formatData"),
                                      prepared.workspace);
                return result;
            }

            void finishPreparation() {
                if (m_runtime.automationTasks().isCancellationRequested(m_taskId)) {
                    m_runtime.automationTasks().cancel(m_taskId);
                    deleteLater();
                    return;
                }

                auto project = m_runtime.project().getProject(m_command.expected.documentId);
                if (!project) {
                    m_runtime.automationTasks().fail(m_taskId, project.getError());
                    deleteLater();
                    return;
                }
                auto document = m_runtime.documents().getDocument(m_command.expected.documentId);
                if (!document) {
                    m_runtime.automationTasks().fail(m_taskId, document.getError());
                    deleteLater();
                    return;
                }
                QSet<int> trackIds;
                for (const auto &track : project.get().tracks)
                    trackIds.insert(track.id.value());

                QList<ClipInsertDto> selected;
                QStringList warnings;
                for (qsizetype index = 0; index < m_entries.size(); ++index) {
                    const auto &entry = m_entries.at(index);
                    QString failure = entry.error;
                    if (failure.isEmpty() && !trackIds.contains(entry.request.trackId.value()))
                        failure = QStringLiteral("Target track does not exist");
                    if (!failure.isEmpty()) {
                        if (m_failurePolicy == PublicBatchFailurePolicy::Atomic) {
                            m_runtime.automationTasks().fail(m_taskId, importError(failure, index));
                            deleteLater();
                            return;
                        }
                        warnings.append(QStringLiteral("Skipped %1: %2")
                                            .arg(entry.request.clip.audioPath, failure));
                        continue;
                    }
                    selected.append(preparedInsert(entry, document.get().path));
                }
                if (selected.isEmpty()) {
                    m_runtime.automationTasks().fail(
                        m_taskId,
                        importError(QStringLiteral("No audio item was decoded successfully"), 0));
                    deleteLater();
                    return;
                }

                auto validationContext = m_command;
                validationContext.validateOnly = true;
                auto validated = m_runtime.project().insertClips(validationContext, selected);
                if (!validated) {
                    m_runtime.automationTasks().fail(m_taskId, validated.getError());
                    deleteLater();
                    return;
                }
                if (m_runtime.automationTasks().isCancellationRequested(m_taskId)) {
                    m_runtime.automationTasks().cancel(m_taskId);
                    deleteLater();
                    return;
                }
                const auto committing = m_runtime.automationTasks().beginCommitting(m_taskId);
                if (!committing || !committing.get()) {
                    if (!committing)
                        m_runtime.automationTasks().fail(m_taskId, committing.getError());
                    deleteLater();
                    return;
                }
                m_command.validateOnly = false;
                auto committed = m_runtime.project().insertClips(m_command, selected);
                if (!committed) {
                    m_runtime.automationTasks().fail(m_taskId, committed.getError());
                    deleteLater();
                    return;
                }
                auto mutation = committed.get();
                mutation.warnings.append(warnings);
                m_runtime.automationTasks().succeed(m_taskId, std::move(mutation));
                deleteLater();
            }

            CoreRuntime &m_runtime;
            AppModel *m_model = nullptr;
            CommandContext m_command;
            PublicBatchFailurePolicy m_failurePolicy = PublicBatchFailurePolicy::Atomic;
            QString m_operationId;
            QList<Entry> m_entries;
            TaskId m_taskId;
            qsizetype m_remaining = 0;
        };

        AutomationResult<TaskAcceptedResult> startAudioImport(
            CoreRuntime &runtime, AppModel *model, CommandContext command,
            QList<ClipInsertDto> clips, const PublicBatchFailurePolicy failurePolicy,
            const QString &operationId) {
            auto *state = new HeadlessAudioImportTask(
                runtime, model, std::move(command), std::move(clips), failurePolicy, operationId,
                QCoreApplication::instance());
            auto result = state->prepare();
            if (!result || result.get().validatedOnly)
                state->deleteLater();
            return result;
        }

        QJsonObject audioExportCapabilities(CoreRuntime &runtime, const DocumentId &documentId) {
            const auto supported = AudioExportAutomationFacade::capabilities();
            const auto toJson = [](const auto &values) {
                QJsonArray result;
                for (const auto &value : values)
                    result.append(value);
                return result;
            };
            QJsonArray sources;
            auto project = runtime.project().getProject(documentId);
            if (project) {
                for (const auto &track : project.get().tracks) {
                    sources.append(QJsonObject{
                        {QStringLiteral("id"), track.id.value()},
                        {QStringLiteral("name"),
                         track.data.name.isEmpty()
                             ? QStringLiteral("Track %1").arg(track.id.value())
                             : track.data.name},
                        {QStringLiteral("kind"), QStringLiteral("track")},
                        {QStringLiteral("available"), true},
                    });
                }
            }
            return {
                {QStringLiteral("formats"), toJson(supported.formats)},
                {QStringLiteral("sample_rates"), toJson(supported.sampleRates)},
                {QStringLiteral("channel_modes"), toJson(supported.channelModes)},
                {QStringLiteral("mixing_modes"), toJson(supported.mixingModes)},
                {QStringLiteral("source_modes"), toJson(supported.sourceModes)},
                {QStringLiteral("sources"), sources},
            };
        }

        QJsonObject extractionCapabilities(CoreRuntime &runtime, SynthrtEngine *engine,
                                           const DocumentId &documentId, const ClipId clipId) {
            bool sourceSupported = false;
            bool sourceReady = false;
            QJsonArray languages;
            auto project = runtime.project().getProject(documentId);
            if (project) {
                for (const auto &track : project.get().tracks) {
                    for (const auto &clip : track.clips) {
                        if (clip.id == clipId) {
                            sourceSupported = clip.data.type == ClipDraftDto::Type::Audio;
                            sourceReady = sourceSupported && QFileInfo(clip.data.audioPath).isFile();
                            for (const auto &language : track.data.singerInfo.languages())
                                languages.append(language.id());
                            break;
                        }
                    }
                }
            }
            auto settings = runtime.settings().getSettings();
            const bool pitchConfigured =
                settings && QFileInfo(settings.get().general.pitchModelPath).isFile();
            const bool midiConfigured =
                settings && QFileInfo(settings.get().general.gameDirectory).isDir();
            if (settings && !settings.get().general.defaultSingingLanguage.isEmpty() &&
                !languages.contains(settings.get().general.defaultSingingLanguage)) {
                languages.append(settings.get().general.defaultSingingLanguage);
            }
            const bool pitchModuleReady = engine && engine->pitchExtractionReady();
            const bool midiModuleReady = engine && engine->midiExtractionReady();
            const auto optionSchema = [](const QString &operationId) {
                const auto *contract = AutomationWire::findPublicTool(operationId);
                const auto options = contract
                                         ? contract->inputSchema
                                               .value(QStringLiteral("properties"))
                                               .toObject()
                                               .value(QStringLiteral("options"))
                                               .toObject()
                                         : AutomationWire::JsonSchema::object();
                return AutomationWire::JsonSchema::document(options);
            };
            const auto model = [](const QString &id, const QString &name, const bool configured,
                                  const bool moduleReady, const QString &configurationReason,
                                  const QString &moduleReason) {
                const bool available = configured && moduleReady;
                return QJsonObject{
                    {QStringLiteral("model_id"), id},
                    {QStringLiteral("display_name"), name},
                    {QStringLiteral("configured"), configured},
                    {QStringLiteral("available"), available},
                    {QStringLiteral("unavailable_reason"),
                     available ? QString()
                               : configured ? moduleReason : configurationReason},
                };
            };
            const auto capabilityReason = [](const bool sourceSupported, const bool sourceReady,
                                             const bool moduleReady, const bool modelConfigured,
                                             const QString &moduleReason,
                                             const QString &modelReason) {
                if (!sourceSupported)
                    return QStringLiteral("The selected clip is not an audio clip");
                if (!sourceReady)
                    return QStringLiteral("The selected audio clip source is unavailable");
                if (!moduleReady)
                    return moduleReason;
                if (!modelConfigured)
                    return modelReason;
                return QString();
            };
            return {
                {QStringLiteral("clip_id"), clipId.value()},
                {QStringLiteral("pitch"),
                 QJsonObject{
                     {QStringLiteral("supported"), true},
                     {QStringLiteral("source_supported"), sourceSupported},
                     {QStringLiteral("available"),
                      sourceReady && pitchModuleReady && pitchConfigured},
                     {QStringLiteral("module_state"),
                      pitchModuleReady ? QStringLiteral("ready")
                                       : QStringLiteral("unavailable")},
                     {QStringLiteral("unavailable_reason"),
                      capabilityReason(sourceSupported, sourceReady, pitchModuleReady,
                                       pitchConfigured,
                                       QStringLiteral("Pitch extraction module is unavailable"),
                                       QStringLiteral("RMVPE model is not configured"))},
                     {QStringLiteral("models"),
                      QJsonArray{model(
                          QStringLiteral("rmvpe"), QStringLiteral("RMVPE"), pitchConfigured,
                          pitchModuleReady, QStringLiteral("RMVPE model is not configured"),
                          QStringLiteral("Pitch extraction module is unavailable"))}},
                     {QStringLiteral("option_schema"),
                      optionSchema(OperationIds::extract::pitch::start)},
                     {QStringLiteral("range_support"),
                      QJsonObject{
                          {QStringLiteral("source_range"),
                           QStringLiteral("visible_audio_clip")},
                          {QStringLiteral("custom_frequency"), false},
                      }},
                 }},
                {QStringLiteral("midi"),
                 QJsonObject{
                     {QStringLiteral("supported"), true},
                     {QStringLiteral("source_supported"), sourceSupported},
                     {QStringLiteral("available"),
                      sourceReady && midiModuleReady && midiConfigured},
                     {QStringLiteral("module_state"),
                      midiModuleReady ? QStringLiteral("ready")
                                      : QStringLiteral("unavailable")},
                     {QStringLiteral("unavailable_reason"),
                      capabilityReason(sourceSupported, sourceReady, midiModuleReady,
                                       midiConfigured,
                                       QStringLiteral("MIDI extraction module is unavailable"),
                                       QStringLiteral("GAME model directory is not configured"))},
                     {QStringLiteral("models"),
                      QJsonArray{model(
                          QStringLiteral("game"), QStringLiteral("GAME"), midiConfigured,
                          midiModuleReady,
                          QStringLiteral("GAME model directory is not configured"),
                          QStringLiteral("MIDI extraction module is unavailable"))}},
                     {QStringLiteral("option_schema"),
                      optionSchema(OperationIds::extract::midi::start)},
                     {QStringLiteral("range_support"),
                      QJsonObject{
                          {QStringLiteral("source_range"),
                           QStringLiteral("visible_audio_clip")},
                          {QStringLiteral("minimum_note_length"),
                           QJsonObject{
                               {QStringLiteral("minimum"), 1},
                               {QStringLiteral("maximum"),
                                std::numeric_limits<int>::max()},
                           }},
                      }},
                 }},
                {QStringLiteral("languages"), languages},
            };
        }

        QList<InferPiece *> inferencePieces(AppModel *model, const QJsonObject &scope) {
            QList<InferPiece *> result;
            if (!model)
                return result;
            QSet<int> trackIds;
            QSet<int> clipIds;
            for (const auto &value : scope.value(QStringLiteral("track_ids")).toArray())
                trackIds.insert(value.toInt());
            for (const auto &value : scope.value(QStringLiteral("clip_ids")).toArray())
                clipIds.insert(value.toInt());
            const auto kind = scope.value(QStringLiteral("kind")).toString();
            for (auto *track : model->tracks()) {
                if (kind == QStringLiteral("track") && !trackIds.contains(track->id()))
                    continue;
                for (auto *clip : track->clips()) {
                    if (clip->clipType() != Clip::Singing ||
                        (kind == QStringLiteral("clip") && !clipIds.contains(clip->id()))) {
                        continue;
                    }
                    result.append(static_cast<SingingClip *>(clip)->pieces());
                }
            }
            return result;
        }

        std::optional<InferenceStage> inferenceStage(const QString &name) {
            if (name == QStringLiteral("duration"))
                return InferenceStage::Duration;
            if (name == QStringLiteral("pitch"))
                return InferenceStage::Pitch;
            if (name == QStringLiteral("variance"))
                return InferenceStage::Variance;
            if (name == QStringLiteral("acoustic"))
                return InferenceStage::Acoustic;
            return std::nullopt;
        }

        AutomationResult<MutationResult> resetInference(CoreRuntime &runtime, AppModel *model,
                                                        PublicInferenceResetRequest request) {
            const auto stage = inferenceStage(request.stage);
            if (!stage) {
                return AutomationError::invalidArgument(QStringLiteral("stage"),
                                                        QStringLiteral("Inference stage is invalid"));
            }
            InferenceMutationRequest mutation;
            mutation.kind = InferenceMutationKind::ResetStage;
            mutation.stage = *stage;
            for (auto *piece : inferencePieces(model, request.scope))
                mutation.pieceIds.append(PieceId(piece->id()));
            if (mutation.pieceIds.isEmpty()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("scope"),
                    QStringLiteral("The inference scope contains no singing pieces"));
            }
            auto result = runtime.inference().applyMutation(request.command, mutation);
            if (!result)
                return result.getError();
            return result.get().mutation;
        }

        AutomationError inferenceError(const QString &message, const QString &fieldPath = {}) {
            AutomationError result;
            result.code = AutomationErrorCode::HostCapabilityUnavailable;
            result.operationId = QStringLiteral("inference.start");
            result.message = message;
            result.fieldPath = fieldPath;
            return result;
        }

        QString inferenceModelId(const InferPiece &piece) {
            const auto &identifier = piece.identifier;
            if (identifier.isEmpty())
                return {};
            return QStringLiteral("%1/%2@%3")
                .arg(identifier.packageId, identifier.singerId,
                     identifier.packageVersion.toString());
        }

        class HeadlessInferenceTask final : public QObject {
        public:
            HeadlessInferenceTask(CoreRuntime &runtime, AppModel *model,
                                  PublicInferenceStartRequest request, QObject *parent)
                : QObject(parent), m_runtime(runtime), m_model(model),
                  m_request(std::move(request)) {
            }

            AutomationResult<TaskAcceptedResult> prepare();

        private:
            void start();
            void requestCancel();
            void handleState(const QString &state);
            void evaluate();
            void finishCanceled();
            void fail(AutomationError error);

            CoreRuntime &m_runtime;
            AppModel *m_model = nullptr;
            PublicInferenceStartRequest m_request;
            QList<QPointer<InferPiece>> m_pieces;
            QString m_earliestStage;
            TaskId m_taskId;
            MutationResult m_mutation;
            bool m_started = false;
            bool m_finished = false;
        };

        AutomationResult<TaskAcceptedResult> HeadlessInferenceTask::prepare() {
            auto base = validateBase(m_runtime, m_request.command);
            if (!base)
                return base.getError();

            const auto selected = inferencePieces(m_model, m_request.scope);
            if (selected.isEmpty()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("scope"),
                    QStringLiteral("The inference scope contains no singing pieces"));
            }
            for (auto *piece : selected) {
                if (!piece || inferenceModelId(*piece).isEmpty()) {
                    return inferenceError(
                        QStringLiteral("A target piece does not have a resolved singer model"),
                        QStringLiteral("scope"));
                }
                m_pieces.append(piece);
            }

            const auto supported = InferenceAutomationFacade::supportedStages();
            if (m_request.stages.isEmpty())
                m_request.stages = supported;
            auto earliest = supported.size();
            for (const auto &stage : std::as_const(m_request.stages)) {
                const auto index = supported.indexOf(stage);
                if (index < 0) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("stages"),
                        QStringLiteral("Inference stage is not supported"));
                }
                earliest = std::min(earliest, index);
            }
            if (earliest >= supported.size()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("stages"),
                    QStringLiteral("At least one inference stage is required"));
            }
            m_earliestStage = supported.at(earliest);

            auto settings = m_runtime.settings().getSettings();
            if (!settings)
                return settings.getError();
            const auto &active = settings.get().inference;
            const auto provider =
                m_request.options.value(QStringLiteral("provider_id")).toString();
            if (!provider.isEmpty() && provider != active.executionProvider) {
                return AutomationError::invalidArgument(
                    QStringLiteral("options.provider_id"),
                    QStringLiteral("The requested inference provider is not active"));
            }
            const auto device =
                m_request.options.value(QStringLiteral("device_id")).toString();
            if (!device.isEmpty() && device != active.selectedGpuId) {
                return AutomationError::invalidArgument(
                    QStringLiteral("options.device_id"),
                    QStringLiteral("The requested inference device is not active"));
            }
            const auto model = m_request.options.value(QStringLiteral("model_id")).toString();
            if (!model.isEmpty() &&
                std::none_of(m_pieces.cbegin(), m_pieces.cend(), [&](const auto &piece) {
                    return piece && inferenceModelId(*piece) == model;
                })) {
                return AutomationError::invalidArgument(
                    QStringLiteral("options.model_id"),
                    QStringLiteral("The requested model is not used by the target scope"));
            }
            if (!inferEngine->initialized())
                return inferenceError(QStringLiteral("The inference engine is not initialized"));
            if (m_request.command.validateOnly)
                return TaskAcceptedResult{{}, base.get(), true};

            const QPointer<HeadlessInferenceTask> weak(this);
            const auto task = m_runtime.automationTasks().createTask(
                QStringLiteral("inference.start"), base.get(), std::nullopt,
                [weak] {
                    if (weak)
                        weak->requestCancel();
                },
                m_request.command.clientId);
            m_taskId = task.taskId;
            m_request.command.taskId = m_taskId;
            for (const auto &piece : std::as_const(m_pieces)) {
                connect(piece, &InferPiece::stateChanged, this,
                        [this](const QString &state) { handleState(state); });
                connect(piece, &QObject::destroyed, this, [this] {
                    fail(inferenceError(
                        QStringLiteral("An inference target was removed while running"),
                        QStringLiteral("scope")));
                });
            }
            QTimer::singleShot(0, this, [weak] {
                if (weak)
                    weak->start();
            });
            return TaskAcceptedResult{m_taskId, base.get(), false};
        }

        void HeadlessInferenceTask::start() {
            if (m_finished)
                return;
            if (!m_runtime.automationTasks().markRunning(m_taskId)) {
                fail(inferenceError(QStringLiteral("The inference task could not start")));
                return;
            }
            auto reset = resetInference(
                m_runtime, m_model,
                {m_request.command, m_request.scope, m_earliestStage});
            if (!reset) {
                fail(reset.getError());
                return;
            }
            m_mutation = reset.get();
            m_started = true;
            for (const auto &piece : std::as_const(m_pieces)) {
                if (!piece) {
                    fail(inferenceError(
                        QStringLiteral("An inference target was removed while starting"),
                        QStringLiteral("scope")));
                    return;
                }
                inferController->restartPieceInference(*piece);
            }
            evaluate();
        }

        void HeadlessInferenceTask::requestCancel() {
            if (m_finished)
                return;
            m_finished = true;
            for (const auto &piece : std::as_const(m_pieces)) {
                if (piece)
                    inferController->cancelPieceInference(piece->id());
            }
            m_runtime.automationTasks().cancel(m_taskId);
            deleteLater();
        }

        void HeadlessInferenceTask::handleState(const QString &state) {
            if (!m_started || m_finished)
                return;
            if (state.endsWith(QStringLiteral(".Error")) ||
                state.endsWith(QStringLiteral(".Dropped"))) {
                fail(inferenceError(
                    QStringLiteral("Inference pipeline failed in state %1").arg(state)));
                return;
            }
            evaluate();
        }

        void HeadlessInferenceTask::evaluate() {
            if (!m_started || m_finished)
                return;
            const auto ready =
                std::all_of(m_pieces.cbegin(), m_pieces.cend(), [](const auto &piece) {
                    return piece && piece->state.get() == QStringLiteral("Ready");
                });
            if (!ready)
                return;
            const auto committing = m_runtime.automationTasks().beginCommitting(m_taskId);
            if (!committing || !committing.get()) {
                if (!committing)
                    fail(committing.getError());
                else
                    finishCanceled();
                return;
            }
            m_finished = true;
            m_mutation.current = m_runtime.documentVersion();
            m_mutation.changed = m_mutation.current != m_mutation.previous;
            m_runtime.automationTasks().succeed(m_taskId, std::move(m_mutation));
            deleteLater();
        }

        void HeadlessInferenceTask::finishCanceled() {
            if (m_finished)
                return;
            m_finished = true;
            m_runtime.automationTasks().cancel(m_taskId);
            deleteLater();
        }

        void HeadlessInferenceTask::fail(AutomationError error) {
            if (m_finished)
                return;
            m_finished = true;
            error.operationId = QStringLiteral("inference.start");
            error.taskId = m_taskId;
            for (const auto &piece : std::as_const(m_pieces)) {
                if (piece)
                    inferController->cancelPieceInference(piece->id());
            }
            m_runtime.automationTasks().fail(m_taskId, std::move(error));
            deleteLater();
        }

        AutomationResult<TaskAcceptedResult> startInference(
            CoreRuntime &runtime, AppModel *model, PublicInferenceStartRequest request) {
            auto *state = new HeadlessInferenceTask(
                runtime, model, std::move(request), QCoreApplication::instance());
            auto result = state->prepare();
            if (!result || result.get().validatedOnly)
                state->deleteLater();
            return result;
        }
    }

    PublicAutomationHostServices createPublicAutomationHostServices(CoreRuntime &runtime,
                                                                    AppModel *model,
                                                                    SynthrtEngine *synthrtEngine) {
        PublicAutomationHostServices services;
        services.openDocument = [&runtime](const PublicDocumentOpenRequest &request) {
            if (request.unsavedPolicy == PublicUnsavedPolicy::Reject) {
                auto state = runtime.history().getState(request.command.expected.documentId);
                if (!state)
                    return AutomationResult<TaskAcceptedResult>(state.getError());
                if (!state.get().onSavePoint) {
                    return AutomationResult<TaskAcceptedResult>(AutomationError::invalidArgument(
                        QStringLiteral("unsaved_policy"),
                        QStringLiteral("The current document has unsaved changes")));
                }
            }
            return startProjectLoad(runtime, request.canonicalPath, {}, ProjectLoadPurpose::Open,
                                    true, true, request.command);
        };
        services.importDocument = [&runtime](const PublicDocumentImportRequest &request) {
            return startProjectLoad(runtime, request.canonicalPath, request.mergeMode,
                                    ProjectLoadPurpose::Import, request.importTempo,
                                    request.importTimeSignature, request.command);
        };
        services.importAudioClip = [&runtime, model](const PublicAudioClipImportRequest &request) {
            return startAudioImport(
                runtime, model, request.command,
                {{request.trackId,
                  audioClipDraft(request.canonicalPath, request.properties, request.clientRef)}},
                PublicBatchFailurePolicy::Atomic, QStringLiteral("audio_clips.import"));
        };
        services.importAudioClips = [&runtime, model](const PublicAudioClipBatchImportRequest &request) {
            QList<ClipInsertDto> clips;
            clips.reserve(request.items.size());
            for (const auto &item : request.items) {
                clips.append({item.trackId,
                              audioClipDraft(item.canonicalPath, item.properties, item.clientRef)});
            }
            return startAudioImport(runtime, model, request.command, std::move(clips),
                                    request.failurePolicy,
                                    QStringLiteral("audio_clips.import_batch"));
        };
        services.prepareAudioPath = [](const QString &canonicalPath) {
            return prepareAudioPath(canonicalPath);
        };
        services.audioExportCapabilities = [&runtime](const DocumentId &documentId) {
            return AutomationResult<QJsonValue>(audioExportCapabilities(runtime, documentId));
        };
        services.extractionCapabilities = [&runtime, synthrtEngine](const DocumentId &documentId,
                                                                    const ClipId clipId) {
            return AutomationResult<QJsonValue>(
                extractionCapabilities(runtime, synthrtEngine, documentId, clipId));
        };
        services.inferenceCapabilities = [&runtime, model](const DocumentId &documentId,
                                                           const QJsonObject &scope) {
            auto project = runtime.project().getProject(documentId);
            if (!project)
                return AutomationResult<QJsonValue>(project.getError());
            const auto pieces = inferencePieces(model, scope);
            QJsonArray stages;
            for (const auto &stage : InferenceAutomationFacade::supportedStages())
                stages.append(stage);
            auto settings = runtime.settings().getSettings();
            QJsonArray providers;
            QJsonArray devices;
            if (settings) {
                const auto &inference = settings.get().inference;
                providers.append(QJsonObject{
                    {QStringLiteral("id"), inference.executionProvider},
                    {QStringLiteral("display_name"), inference.executionProvider},
                    {QStringLiteral("available"), !inference.executionProvider.isEmpty()},
                    {QStringLiteral("unavailable_reason"),
                     inference.executionProvider.isEmpty()
                         ? QStringLiteral("No inference provider is configured")
                         : QString()},
                });
                if (!inference.selectedGpuId.isEmpty()) {
                    devices.append(QJsonObject{
                        {QStringLiteral("id"), inference.selectedGpuId},
                        {QStringLiteral("display_name"), inference.selectedGpuId},
                        {QStringLiteral("available"), true},
                        {QStringLiteral("unavailable_reason"), QString()},
                    });
                }
            }
            QJsonArray models;
            QSet<QString> modelIds;
            const auto engineReady = inferEngine->initialized();
            for (const auto *piece : pieces) {
                const auto &identifier = piece->identifier;
                const auto id = QStringLiteral("%1/%2@%3")
                                    .arg(identifier.packageId, identifier.singerId,
                                         identifier.packageVersion.toString());
                if (identifier.isEmpty() || modelIds.contains(id))
                    continue;
                modelIds.insert(id);
                models.append(QJsonObject{
                    {QStringLiteral("model_id"), id},
                    {QStringLiteral("display_name"), identifier.singerId},
                    {QStringLiteral("available"), engineReady},
                    {QStringLiteral("unavailable_reason"),
                     engineReady ? QString()
                                 : QStringLiteral("The inference engine is not initialized")},
                });
            }
            return AutomationResult<QJsonValue>(QJsonObject{
                {QStringLiteral("scope"), scope},
                {QStringLiteral("stages"), stages},
                {QStringLiteral("providers"), providers},
                {QStringLiteral("devices"), devices},
                {QStringLiteral("models"), models},
            });
        };
        services.resetInferenceStage = [&runtime, model](const PublicInferenceResetRequest &request) {
            return resetInference(runtime, model, request);
        };
        services.startInference = [&runtime, model](const PublicInferenceStartRequest &request) {
            return startInference(runtime, model, request);
        };
        return services;
    }

} // namespace Automation
