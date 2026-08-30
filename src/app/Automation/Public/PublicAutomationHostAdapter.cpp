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
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QSaveFile>
#include <QSet>
#include <QThreadPool>
#include <QTimer>
#include <QTemporaryDir>

#include <algorithm>
#include <atomic>
#include <cmath>
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

        struct PreparedAudioPath {
            QString sha512;
            QJsonObject formatData;
        };

        AutomationResult<PreparedAudioPath> prepareAudioPath(const QString &path) {
            std::unique_ptr<DecodeAudioTask> probe(AudioFilePreparer::createPrepareTask(path));
            if (!probe || !probe->io) {
                return audioPathPreparationError(
                    QStringLiteral("No audio decoder is available for the selected file"));
            }
            const auto workspace = probe->workspace;
            probe.reset();

            QTemporaryDir snapshotDirectory;
            if (!snapshotDirectory.isValid())
                return audioPathPreparationError(
                    QStringLiteral("Failed to create an audio snapshot"));
            const auto snapshotPath =
                QDir(snapshotDirectory.path()).filePath(QFileInfo(path).fileName());

            auto hashTask = std::make_unique<ComputeAudioHashTask>();
            hashTask->path = path;
            hashTask->snapshotPath = snapshotPath;
            static_cast<QRunnable *>(hashTask.get())->run();
            if (!hashTask->success || hashTask->resultSha512.isEmpty()) {
                return audioPathPreparationError(QStringLiteral("Failed to compute audio hash"));
            }

            std::unique_ptr<DecodeAudioTask> decodeTask(
                AudioFilePreparer::createPrepareTask(snapshotPath, nullptr, workspace));
            if (!decodeTask || !decodeTask->io)
                return audioPathPreparationError(
                    QStringLiteral("Failed to open the audio snapshot"));
            static_cast<QRunnable *>(decodeTask.get())->run();
            if (!decodeTask->success || decodeTask->terminated())
                return audioPathPreparationError(QStringLiteral("Audio decoding failed"));
            return PreparedAudioPath{hashTask->resultSha512, workspace};
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

        AutomationResult<QString>
            materializeProjectSnapshot(std::unique_ptr<QTemporaryDir> &directory,
                                       const QString &sourcePath, const QByteArray &bytes,
                                       const qsizetype sequence) {
            if (!directory)
                directory = std::make_unique<QTemporaryDir>();
            if (!directory->isValid()) {
                return AutomationError{
                    .code = AutomationErrorCode::IoError,
                    .message = QStringLiteral("A project snapshot directory could not be created"),
                };
            }
            const auto snapshotPath =
                QDir(directory->path())
                    .filePath(QStringLiteral("%1-%2")
                                  .arg(sequence)
                                  .arg(QFileInfo(sourcePath).fileName()));
            QSaveFile snapshot(snapshotPath);
            if (!snapshot.open(QIODevice::WriteOnly) || snapshot.write(bytes) != bytes.size() ||
                !snapshot.commit()) {
                return AutomationError{
                    .code = AutomationErrorCode::IoError,
                    .message = QStringLiteral("A project snapshot could not be written"),
                };
            }
            return snapshotPath;
        }

        class HeadlessProjectLoadTask final : public QObject {
        public:
            HeadlessProjectLoadTask(
                CoreRuntime &runtime, QString path, QString mergeMode,
                const ProjectLoadPurpose purpose, QByteArray encoding, const bool importTempo,
                const bool importTimeSignature, CommandContext command,
                PublicProjectInputRevalidator revalidatePlan, QObject *parent)
                : QObject(parent), m_runtime(runtime), m_path(std::move(path)),
                  m_mergeMode(std::move(mergeMode)), m_purpose(purpose),
                  m_encoding(std::move(encoding)), m_importTempo(importTempo),
                  m_importTimeSignature(importTimeSignature), m_command(std::move(command)),
                  m_revalidatePlan(std::move(revalidatePlan)) {
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
                const bool supported = m_purpose == ProjectLoadPurpose::Open ? descriptor.canOpen
                                                                             : descriptor.canImport;
                if (!supported) {
                    AutomationError error;
                    error.code = AutomationErrorCode::FormatUnsupported;
                    error.fieldPath = QStringLiteral("path");
                    error.message =
                        QStringLiteral("The project format does not support this operation");
                    return error;
                }
                m_handler = handler;
                if (m_command.validateOnly)
                    return TaskAcceptedResult{{}, base.get(), true};

                const auto operation = m_purpose == ProjectLoadPurpose::Open
                                           ? OperationIds::documents::open
                                           : OperationIds::documents::import_document;
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
                if (!m_runtime.automationTasks().markRunning(m_taskId)) {
                    auto error =
                        unavailable(QStringLiteral("The project load task could not start"));
                    error.taskId = m_taskId;
                    return error;
                }
                QTimer::singleShot(0, this, [this] { start(); });
                return TaskAcceptedResult{m_taskId, base.get(), false};
            }

        private:
            void start() {
                if (m_runtime.automationTasks().isCancellationRequested(m_taskId)) {
                    m_runtime.automationTasks().cancel(m_taskId);
                    deleteLater();
                    return;
                }
                QString loadPath = m_path;
                if (m_revalidatePlan) {
                    auto revalidated = m_revalidatePlan();
                    if (!revalidated) {
                        fail(revalidated.getError());
                        return;
                    }
                    if (revalidated.get()) {
                        auto snapshot = materializeProjectSnapshot(
                            m_snapshotDirectory, m_path, *revalidated.get(), 0);
                        if (!snapshot) {
                            fail(snapshot.getError());
                            return;
                        }
                        loadPath = snapshot.get();
                    }
                }

                ProjectLoadRequest request{
                    .filePath = loadPath,
                    .purpose = m_purpose,
                    .requestId = 1,
                    .interactive = false,
                    .encoding = m_encoding,
                    .importTempo = m_importTempo,
                    .importTimeSignature = m_importTimeSignature,
                };
                m_session = m_handler ? m_handler->createSession(request, nullptr, this) : nullptr;
                if (!m_session) {
                    fail(unavailable(
                        QStringLiteral("The project format could not create a load session")));
                    return;
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
                m_session->start();
            }

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
                AutomationResult<MutationResult> result =
                    unavailable(QStringLiteral("The project loader returned no project"));
                if (auto *replace = std::get_if<ReplaceProjectPayload>(&prepared)) {
                    const auto draft = documentDraftDto(replace->model, replace->loopSettings);
                    result = m_runtime.documents().commitOpenedDocument(
                        m_command, draft,
                        replace->sourceKind == ProjectSourceKind::Native ? m_path : QString(),
                        replace->sourceKind == ProjectSourceKind::Native
                            ? QFileInfo(m_path).fileName()
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

            void fail(AutomationError failure) {
                failure.taskId = m_taskId;
                failure.operationId = m_purpose == ProjectLoadPurpose::Open
                                          ? OperationIds::documents::open
                                          : OperationIds::documents::import_document;
                m_runtime.automationTasks().fail(m_taskId, std::move(failure));
                deleteLater();
            }

            CoreRuntime &m_runtime;
            QString m_path;
            QString m_mergeMode;
            ProjectLoadPurpose m_purpose;
            QByteArray m_encoding;
            bool m_importTempo = true;
            bool m_importTimeSignature = true;
            CommandContext m_command;
            PublicProjectInputRevalidator m_revalidatePlan;
            TaskId m_taskId;
            IProjectFormatHandler *m_handler = nullptr;
            QPointer<IProjectLoadSession> m_session;
            std::unique_ptr<QTemporaryDir> m_snapshotDirectory;
        };

        AutomationResult<TaskAcceptedResult>
            startProjectLoad(CoreRuntime &runtime, const QString &path, const QString &mergeMode,
                             const ProjectLoadPurpose purpose, QByteArray encoding,
                             const bool importTempo, const bool importTimeSignature,
                             CommandContext command,
                             PublicProjectInputRevalidator revalidatePlan) {
            auto *state = new HeadlessProjectLoadTask(
                runtime, path, mergeMode, purpose, std::move(encoding), importTempo,
                importTimeSignature, std::move(command), std::move(revalidatePlan),
                QCoreApplication::instance());
            auto result = state->prepare();
            if (!result || result.get().validatedOnly)
                state->deleteLater();
            return result;
        }

        class HeadlessProjectBatchLoadTask final : public QObject {
        public:
            HeadlessProjectBatchLoadTask(CoreRuntime &runtime, AppModel *model,
                                         PublicDocumentBatchImportRequest request, QObject *parent)
                : QObject(parent), m_runtime(runtime), m_model(model),
                  m_request(std::move(request)) {
            }

            AutomationResult<TaskAcceptedResult> prepare() {
                auto base = validateBase(m_runtime, m_request.command);
                if (!base)
                    return base.getError();
                if (!m_model)
                    return unavailable(QStringLiteral("The project model is unavailable"));

                for (const auto &item : std::as_const(m_request.items)) {
                    const auto validation = validateItem(item);
                    if (!validation && m_request.failurePolicy == PublicBatchFailurePolicy::Atomic)
                        return validation.getError();
                }
                if (m_request.command.validateOnly)
                    return TaskAcceptedResult{{}, base.get(), true};

                m_baseTimeline = m_model->timeline();
                const QPointer<HeadlessProjectBatchLoadTask> weak(this);
                const auto task = m_runtime.automationTasks().createTask(
                    OperationIds::documents::import_batch, base.get(), std::nullopt,
                    [weak] {
                        if (weak)
                            weak->cancel();
                    },
                    m_request.command.clientId);
                m_taskId = task.taskId;
                m_request.command.taskId = m_taskId;
                if (!m_runtime.automationTasks().markRunning(m_taskId)) {
                    auto failure = unavailable(
                        QStringLiteral("The project batch import task could not start"));
                    failure.taskId = m_taskId;
                    return failure;
                }
                QTimer::singleShot(0, this, [this] { startNext(); });
                return TaskAcceptedResult{m_taskId, base.get(), false};
            }

        private:
            AutomationResult<IProjectFormatHandler *>
                validateItem(const PublicDocumentBatchImportItem &item) const {
                if (item.validationError)
                    return *item.validationError;
                auto *handler = projectFormatRegistry->resolveByPath(item.canonicalPath);
                if (!handler) {
                    AutomationError failure;
                    failure.code = AutomationErrorCode::FormatUnsupported;
                    failure.fieldPath = QStringLiteral("items.path");
                    failure.message =
                        QStringLiteral("No project format handler accepts an import item");
                    return failure;
                }
                const auto descriptor = handler->descriptor();
                if (!descriptor.canImport ||
                    (!item.formatId.isEmpty() && descriptor.id != item.formatId)) {
                    AutomationError failure;
                    failure.code = AutomationErrorCode::FormatUnsupported;
                    failure.fieldPath = QStringLiteral("items.format_id");
                    failure.message =
                        QStringLiteral("The selected project format cannot import this item");
                    return failure;
                }
                return handler;
            }

            void cancel() {
                if (m_session)
                    m_session->cancel();
            }

            void startNext() {
                if (m_finished)
                    return;
                if (m_runtime.automationTasks().isCancellationRequested(m_taskId)) {
                    finishCanceled();
                    return;
                }
                if (m_index >= m_request.items.size()) {
                    commit();
                    return;
                }

                const auto &item = m_request.items.at(m_index);
                auto handler = validateItem(item);
                if (!handler) {
                    handleFailure(handler.getError());
                    return;
                }
                QString loadPath = item.canonicalPath;
                if (item.revalidatePlan) {
                    auto revalidated = item.revalidatePlan();
                    if (!revalidated) {
                        handleFailure(revalidated.getError());
                        return;
                    }
                    if (revalidated.get()) {
                        auto snapshot = materializeProjectSnapshot(
                            m_snapshotDirectory, item.canonicalPath, *revalidated.get(), m_index);
                        if (!snapshot) {
                            handleFailure(snapshot.getError());
                            return;
                        }
                        loadPath = snapshot.get();
                    }
                }
                const auto options = item.options;
                ProjectLoadRequest loadRequest{
                    .filePath = loadPath,
                    .purpose = ProjectLoadPurpose::Import,
                    .requestId = static_cast<quint64>(m_index + 1),
                    .interactive = false,
                    .encoding = options.value(QStringLiteral("encoding")).toString().toLatin1(),
                    .importTempo = options.value(QStringLiteral("import_tempo")).toBool(true),
                    .importTimeSignature =
                        options.value(QStringLiteral("import_time_signatures")).toBool(true),
                };
                m_session = handler.get()->createSession(loadRequest, nullptr, this);
                if (!m_session) {
                    handleFailure(unavailable(
                        QStringLiteral("A project format could not create an import session")));
                    return;
                }
                connect(m_session, &IProjectLoadSession::ready, this,
                        [this] { collect(m_session->takeResult()); });
                connect(m_session, &IProjectLoadSession::failed, this,
                        [this](const ProjectOperationError &source) {
                            handleFailure(projectLoadError(source, m_taskId));
                        });
                connect(m_session, &IProjectLoadSession::canceled, this,
                        [this] { finishCanceled(); });
                m_session->start();
            }

            void collect(PreparedProject prepared) {
                if (m_finished)
                    return;
                auto *append = std::get_if<AppendProjectPayload>(&prepared);
                if (!append) {
                    handleFailure(unavailable(QStringLiteral(
                        "A batch import item returned a non-import project payload")));
                    return;
                }

                const auto requestIndex = m_index;
                const auto options = m_request.items.at(requestIndex).options;
                const auto importTempo =
                    options.value(QStringLiteral("import_tempo")).toBool(true) &&
                    append->importTempo;
                const auto importTimeSignatures =
                    options.value(QStringLiteral("import_time_signatures")).toBool(true) &&
                    append->importTimeSignature;
                m_loadedItems.append({requestIndex, documentDraftDto(append->model), importTempo,
                                      importTimeSignatures});
                advance();
            }

            void handleFailure(AutomationError failure) {
                if (m_finished)
                    return;
                if (m_request.failurePolicy == PublicBatchFailurePolicy::Atomic) {
                    failure.taskId = m_taskId;
                    m_runtime.automationTasks().fail(m_taskId, std::move(failure));
                    finish();
                    return;
                }
                m_warnings.append(failure.message);
                advance();
            }

            void advance() {
                if (m_session) {
                    m_session->deleteLater();
                    m_session = nullptr;
                }
                ++m_index;
                QTimer::singleShot(0, this, [this] { startNext(); });
            }

            void commit() {
                BatchImportDraftDto batch;
                batch.timeline = m_baseTimeline;
                bool importedTempo = false;
                bool importedTimeSignatures = false;
                for (auto &loaded : m_loadedItems) {
                    const auto &requestItem = m_request.items.at(loaded.requestIndex);
                    if (loaded.importTempo && !importedTempo) {
                        batch.timeline.setTempos(loaded.draft.timeline.tempos());
                        importedTempo = true;
                    }
                    if (loaded.importTimeSignatures && !importedTimeSignatures) {
                        batch.timeline.setTimeSignatures(
                            loaded.draft.timeline.timeSignatures());
                        importedTimeSignatures = true;
                    }
                    if (loaded.draft.tracks.isEmpty())
                        m_warnings.append(QStringLiteral("An import item contained no tracks"));
                    for (auto track : loaded.draft.tracks) {
                        BatchImportItemDraftDto item;
                        item.clips = track.clips;
                        track.clips.clear();
                        item.newTrack = std::move(track);
                        if (!item.clips.isEmpty())
                            batch.items.append(std::move(item));
                    }
                }
                const auto committing = m_runtime.automationTasks().beginCommitting(m_taskId);
                if (!committing || !committing.get()) {
                    if (!committing)
                        m_runtime.automationTasks().fail(m_taskId, committing.getError());
                    finish();
                    return;
                }
                auto result = m_runtime.project().commitBatchImport(m_request.command, batch);
                if (result) {
                    auto mutation = result.get();
                    mutation.warnings.append(m_warnings);
                    m_runtime.automationTasks().succeed(m_taskId, std::move(mutation));
                } else {
                    auto failure = result.getError();
                    failure.taskId = m_taskId;
                    m_runtime.automationTasks().fail(m_taskId, std::move(failure));
                }
                finish();
            }

            void finishCanceled() {
                if (!m_finished)
                    m_runtime.automationTasks().cancel(m_taskId);
                finish();
            }

            void finish() {
                if (m_finished)
                    return;
                m_finished = true;
                deleteLater();
            }

            CoreRuntime &m_runtime;
            AppModel *m_model = nullptr;
            PublicDocumentBatchImportRequest m_request;
            struct LoadedItem {
                qsizetype requestIndex = 0;
                DocumentDraftDto draft;
                bool importTempo = false;
                bool importTimeSignatures = false;
            };
            Timeline m_baseTimeline;
            QList<LoadedItem> m_loadedItems;
            QStringList m_warnings;
            TaskId m_taskId;
            qsizetype m_index = 0;
            bool m_finished = false;
            QPointer<IProjectLoadSession> m_session;
            std::unique_ptr<QTemporaryDir> m_snapshotDirectory;
        };

        AutomationResult<TaskAcceptedResult>
            startProjectBatchLoad(CoreRuntime &runtime, AppModel *model,
                                  PublicDocumentBatchImportRequest request) {
            auto *state = new HeadlessProjectBatchLoadTask(runtime, model, std::move(request),
                                                           QCoreApplication::instance());
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

        QByteArray audioImportFingerprint(const QList<ClipInsertDto> &clips,
                                          const QStringList &validationErrors,
                                          const PublicBatchFailurePolicy failurePolicy) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(fingerprint(clips));
            hash.addData(failurePolicy == PublicBatchFailurePolicy::Atomic
                             ? QByteArrayLiteral("atomic")
                             : QByteArrayLiteral("best_effort"));
            hash.addData("\0", 1);
            hash.addData(QByteArray::number(validationErrors.size()));
            for (const auto &validationError : validationErrors) {
                const auto bytes = validationError.toUtf8();
                hash.addData("\0", 1);
                hash.addData(QByteArray::number(bytes.size()));
                hash.addData("\0", 1);
                hash.addData(bytes);
            }
            return hash.result();
        }

        class HeadlessAudioImportTask final : public QObject {
        public:
            HeadlessAudioImportTask(
                CoreRuntime &runtime, AppModel *model, CommandContext command,
                QList<ClipInsertDto> clips, QStringList validationErrors,
                const PublicBatchFailurePolicy failurePolicy, QString operationId,
                QByteArray requestFingerprint, QObject *parent)
                : QObject(parent), m_runtime(runtime), m_model(model),
                  m_command(std::move(command)), m_failurePolicy(failurePolicy),
                  m_operationId(std::move(operationId)),
                  m_requestFingerprint(std::move(requestFingerprint)) {
                m_entries.reserve(clips.size());
                for (qsizetype index = 0; index < clips.size(); ++index) {
                    Entry entry;
                    entry.request = std::move(clips[index]);
                    if (index < validationErrors.size())
                        entry.error = validationErrors.at(index);
                    m_entries.append(std::move(entry));
                }
            }

            AutomationResult<TaskAcceptedResult> prepare() {
                auto base = validateBase(m_runtime, m_command);
                if (!base)
                    return base.getError();
                if (!m_model || m_entries.isEmpty()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("items"),
                        QStringLiteral("No audio import items were supplied"));
                }

                for (qsizetype index = 0; index < m_entries.size(); ++index) {
                    auto &entry = m_entries[index];
                    if (!entry.error.isEmpty())
                        continue;
                    std::unique_ptr<DecodeAudioTask> probe(
                        AudioFilePreparer::createPrepareTask(entry.request.clip.audioPath));
                    if (!probe || !probe->io) {
                        const auto path = entry.request.clip.audioPath;
                        entry.error =
                            QStringLiteral("No audio decoder is available for %1").arg(path);
                        continue;
                    }
                    entry.workspace = probe->workspace;
                    if (m_command.validateOnly)
                        continue;
                    if (!m_snapshotDirectory)
                        m_snapshotDirectory = std::make_unique<QTemporaryDir>();
                    if (!m_snapshotDirectory->isValid()) {
                        entry.error = QStringLiteral("Failed to create an audio snapshot");
                        continue;
                    }
                    entry.snapshotPath =
                        QDir(m_snapshotDirectory->path())
                            .filePath(QStringLiteral("%1-%2").arg(index).arg(
                                QFileInfo(entry.request.clip.audioPath).fileName()));
                    entry.hashTask = new ComputeAudioHashTask;
                    entry.hashTask->path = entry.request.clip.audioPath;
                    entry.hashTask->snapshotPath = entry.snapshotPath;
                }

                if (m_command.validateOnly) {
                    const auto invalid =
                        std::find_if(m_entries.cbegin(), m_entries.cend(), [](const Entry &entry) {
                            return !entry.error.isEmpty();
                        });
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
                const TaskAcceptedResult accepted{m_taskId, base.get(), false};
                if (!m_command.idempotencyKey.isEmpty()) {
                    const auto context = m_command;
                    const auto operationId = m_operationId;
                    const auto requestFingerprint = m_requestFingerprint;
                    const auto bound = m_runtime.automationTasks().setUnsuccessfulCallback(
                        m_taskId,
                        [runtime = &m_runtime, context, operationId, requestFingerprint,
                         accepted](const AutomationTaskSnapshot &) {
                            runtime->dispatcher().releaseDocumentIdempotency(
                                operationId, context, requestFingerprint, accepted);
                        });
                    Q_ASSERT(bound);
                    m_command.idempotencyKey.clear();
                }

                m_remaining = 0;
                for (auto &entry : m_entries) {
                    if (entry.hashTask) {
                        ++m_remaining;
                        auto *hashTask = entry.hashTask;
                        connect(
                            hashTask, &Task::finished, this,
                            [this, hashTask] { handleHashFinished(hashTask); },
                            Qt::QueuedConnection);
                        QThreadPool::globalInstance()->start(hashTask);
                    }
                }
                if (m_remaining == 0)
                    QTimer::singleShot(0, this, [this] { finishPreparation(); });
                return accepted;
            }

        private:
            struct Entry {
                ClipInsertDto request;
                DecodeAudioTask *task = nullptr;
                ComputeAudioHashTask *hashTask = nullptr;
                std::optional<PreparedAudioItem> prepared;
                QJsonObject workspace;
                QString snapshotPath;
                QString sha512;
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
                    if (entry.hashTask)
                        entry.hashTask->terminate();
                }
            }

            void handleFinished(DecodeAudioTask *task) {
                const auto found =
                    std::find_if(m_entries.begin(), m_entries.end(),
                                 [task](const Entry &entry) { return entry.task == task; });
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
                    found->prepared->path = found->request.clip.audioPath;
                }
                found->task = nullptr;
                task->deleteLater();
                if (--m_remaining == 0)
                    finishPreparation();
            }

            void handleHashFinished(ComputeAudioHashTask *task) {
                const auto found =
                    std::find_if(m_entries.begin(), m_entries.end(),
                                 [task](const Entry &entry) { return entry.hashTask == task; });
                if (found == m_entries.end())
                    return;
                if (task->terminated()) {
                    if (found->error.isEmpty())
                        found->error = QStringLiteral("Audio hashing was canceled");
                } else if (!task->success || task->resultSha512.isEmpty()) {
                    if (found->error.isEmpty())
                        found->error = QStringLiteral("Failed to compute audio hash");
                } else {
                    found->sha512 = task->resultSha512;
                    if (!m_runtime.automationTasks().isCancellationRequested(m_taskId)) {
                        found->task = AudioFilePreparer::createPrepareTask(
                            found->snapshotPath, nullptr, found->workspace);
                        if (!found->task || !found->task->io) {
                            delete found->task;
                            found->task = nullptr;
                            found->error = QStringLiteral("Failed to open the audio snapshot");
                        } else {
                            ++m_remaining;
                            auto *decodeTask = found->task;
                            connect(
                                decodeTask, &Task::finished, this,
                                [this, decodeTask] { handleFinished(decodeTask); },
                                Qt::QueuedConnection);
                            QThreadPool::globalInstance()->start(decodeTask);
                        }
                    }
                }
                found->hashTask = nullptr;
                task->deleteLater();
                if (--m_remaining == 0)
                    finishPreparation();
            }

            AutomationError geometryError(const qsizetype index, const QString &message) const {
                AutomationError result;
                result.code = AutomationErrorCode::InvalidArgument;
                result.operationId = m_operationId;
                result.taskId = m_taskId;
                result.fieldPath = m_entries.size() == 1
                                       ? QStringLiteral("start")
                                       : QStringLiteral("items[%1].start").arg(index);
                result.message = message;
                return result;
            }

            AutomationResult<ClipInsertDto> preparedInsert(const Entry &entry,
                                                           const QString &projectPath,
                                                           const qsizetype index) const {
                auto result = entry.request;
                const auto &prepared = *entry.prepared;
                auto &clip = result.clip;
                auto &properties = clip.properties;
                if (!validate(clip)) {
                    return geometryError(index,
                                         QStringLiteral("Audio clip geometry is out of bounds"));
                }
                if (!std::isfinite(prepared.durationMs) || prepared.durationMs < 0.0) {
                    return geometryError(index,
                                         QStringLiteral("Decoded audio duration is invalid"));
                }
                const auto &timeline = m_model->timeline();
                const auto start = properties.start;
                const auto startMs = timeline.tickToMs(start);
                const auto materialEndMs = startMs + prepared.durationMs;
                const auto materialEndTick = timeline.msToTick(materialEndMs);
                if (!std::isfinite(startMs) || !std::isfinite(materialEndMs) ||
                    !std::isfinite(materialEndTick) ||
                    materialEndTick < std::numeric_limits<int>::min() ||
                    materialEndTick > std::numeric_limits<int>::max()) {
                    return geometryError(index,
                                         QStringLiteral("Decoded audio range is out of bounds"));
                }
                const auto materialTicks =
                    std::max<qint64>(1, std::llround(materialEndTick) - static_cast<qint64>(start));
                if (materialTicks > std::numeric_limits<int>::max()) {
                    return geometryError(index,
                                         QStringLiteral("Decoded audio length is out of bounds"));
                }
                if (properties.name.isEmpty())
                    properties.name = QFileInfo(prepared.path).baseName();
                if (properties.length <= 0)
                    properties.length = static_cast<int>(materialTicks);
                if (properties.clipLen <= 0) {
                    properties.clipLen = static_cast<int>(std::max<qint64>(
                        1, static_cast<qint64>(properties.length) - properties.clipStart));
                }
                if (!validate(clip)) {
                    return geometryError(index,
                                         QStringLiteral("Audio clip geometry is out of bounds"));
                }
                const auto visibleStart = static_cast<qint64>(start) + properties.clipStart;
                const auto visibleEnd = visibleStart + properties.clipLen;
                properties.trimStartMs = timeline.tickToMs(visibleStart) - startMs;
                properties.playLengthMs =
                    std::min(timeline.tickToMs(visibleEnd) - timeline.tickToMs(visibleStart),
                             std::max(0.0, prepared.durationMs - properties.trimStartMs));
                properties.materialLengthMs = prepared.durationMs;
                if (!validate(clip)) {
                    return geometryError(index,
                                         QStringLiteral("Audio clip timing is out of bounds"));
                }
                clip.audioPath = prepared.path;
                clip.audioInfo = prepared.audioInfo;
                clip.audioPathInfo = {
                    DiffscopeAudioWorkspace::relativeDirFor(prepared.path, projectPath),
                    entry.sha512};
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
                    auto prepared = preparedInsert(entry, document.get().path, index);
                    if (!prepared) {
                        if (m_failurePolicy == PublicBatchFailurePolicy::Atomic) {
                            m_runtime.automationTasks().fail(m_taskId, prepared.getError());
                            deleteLater();
                            return;
                        }
                        warnings.append(
                            QStringLiteral("Skipped %1: %2")
                                .arg(entry.request.clip.audioPath, prepared.getError().message));
                        continue;
                    }
                    selected.append(prepared.get());
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
            QByteArray m_requestFingerprint;
            QList<Entry> m_entries;
            std::unique_ptr<QTemporaryDir> m_snapshotDirectory;
            TaskId m_taskId;
            qsizetype m_remaining = 0;
        };

        AutomationResult<TaskAcceptedResult> startAudioImport(
            CoreRuntime &runtime, AppModel *model, CommandContext command,
            QList<ClipInsertDto> clips, QStringList validationErrors,
            const PublicBatchFailurePolicy failurePolicy, const QString &operationId) {
            const auto requestFingerprint = command.idempotencyKey.isEmpty()
                                                ? QByteArray{}
                                                : audioImportFingerprint(
                                                      clips, validationErrors, failurePolicy);
            const auto context = command;
            auto launch = [&runtime, model, command = std::move(command), clips = std::move(clips),
                           validationErrors = std::move(validationErrors), failurePolicy, operationId,
                           requestFingerprint]() mutable {
                auto *state = new HeadlessAudioImportTask(
                    runtime, model, std::move(command), std::move(clips),
                    std::move(validationErrors), failurePolicy, operationId, requestFingerprint,
                    QCoreApplication::instance());
                auto result = state->prepare();
                if (!result || result.get().validatedOnly)
                    state->deleteLater();
                return result;
            };
            if (context.idempotencyKey.isEmpty())
                return launch();
            return runtime.dispatcher().dispatchIdempotentDocumentCommandResult<TaskAcceptedResult>(
                operationId, context, requestFingerprint,
                [launch = std::move(launch)](DocumentSession &, const bool) mutable {
                    return launch();
                });
        }

        class HeadlessAudioPathUpdateTask final : public QObject {
        public:
            HeadlessAudioPathUpdateTask(CoreRuntime &runtime, PublicAudioPathUpdateRequest request,
                                        QObject *parent)
                : QObject(parent), m_runtime(runtime), m_request(std::move(request)) {
            }

            AutomationResult<TaskAcceptedResult> prepare() {
                auto base = validateBase(m_runtime, m_request.command);
                if (!base)
                    return base.getError();
                if (m_request.canonicalPath.isEmpty()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("path"), QStringLiteral("Audio path is empty"));
                }
                auto validationContext = m_request.command;
                validationContext.validateOnly = true;
                auto validated = apply(validationContext, {});
                if (!validated)
                    return validated.getError();

                const auto operationId = this->operationId();
                const auto task = m_runtime.automationTasks().createTask(
                    operationId, base.get(),
                    ObjectRef{ObjectKind::Clip, m_request.clipId.value()},
                    [guard = QPointer<HeadlessAudioPathUpdateTask>(this)] {
                        if (guard)
                            guard->m_cancelRequested.store(true, std::memory_order_release);
                    },
                    m_request.command.clientId);
                m_taskId = task.taskId;
                m_request.command.taskId = m_taskId;
                if (!m_runtime.automationTasks().markRunning(m_taskId))
                    return AutomationError::taskNotFound(m_taskId);

                QThreadPool::globalInstance()->start(
                    [guard = QPointer<HeadlessAudioPathUpdateTask>(this),
                     path = m_request.canonicalPath] {
                        auto prepared = prepareAudioPath(path);
                        if (!guard)
                            return;
                        QMetaObject::invokeMethod(
                            guard.data(),
                            [guard, prepared = std::move(prepared)]() mutable {
                                if (guard)
                                    guard->finish(std::move(prepared));
                            },
                            Qt::QueuedConnection);
                    });
                return TaskAcceptedResult{m_taskId, base.get(), false};
            }

        private:
            QString operationId() const {
                return m_request.mode == PublicAudioPathUpdateMode::Relocate
                           ? OperationIds::audio_clips::relocate
                           : OperationIds::audio_clips::confirm_path;
            }

            AutomationResult<MutationResult> apply(const CommandContext &context,
                                                   const PreparedAudioPath &prepared) {
                const AudioPathInfo pathInfo{{}, prepared.sha512};
                if (m_request.mode == PublicAudioPathUpdateMode::Relocate) {
                    return m_runtime.project().relocateAudioClip(
                        context, m_request.clipId, m_request.canonicalPath, pathInfo,
                        prepared.formatData);
                }
                return m_runtime.project().confirmAudioClipPath(
                    context, m_request.clipId, m_request.canonicalPath, pathInfo,
                    prepared.formatData);
            }

            void fail(AutomationError failure) {
                failure.operationId = operationId();
                failure.taskId = m_taskId;
                m_runtime.automationTasks().fail(m_taskId, std::move(failure));
                deleteLater();
            }

            void finish(AutomationResult<PreparedAudioPath> prepared) {
                if (m_cancelRequested.load(std::memory_order_acquire) ||
                    m_runtime.automationTasks().isCancellationRequested(m_taskId)) {
                    m_runtime.automationTasks().cancel(m_taskId);
                    deleteLater();
                    return;
                }
                if (!prepared) {
                    fail(prepared.getError());
                    return;
                }
                auto validationContext = m_request.command;
                validationContext.validateOnly = true;
                auto validated = apply(validationContext, prepared.get());
                if (!validated) {
                    fail(validated.getError());
                    return;
                }
                const auto committing = m_runtime.automationTasks().beginCommitting(m_taskId);
                if (!committing || !committing.get()) {
                    if (!committing)
                        fail(committing.getError());
                    else
                        deleteLater();
                    return;
                }

                m_request.command.validateOnly = false;
                auto committed = apply(m_request.command, prepared.get());
                if (!committed) {
                    fail(committed.getError());
                    return;
                }
                m_runtime.automationTasks().succeed(m_taskId, committed.get());
                deleteLater();
            }

            CoreRuntime &m_runtime;
            PublicAudioPathUpdateRequest m_request;
            TaskId m_taskId;
            std::atomic_bool m_cancelRequested{false};
        };

        AutomationResult<TaskAcceptedResult>
            startAudioPathUpdate(CoreRuntime &runtime, const PublicAudioPathUpdateRequest &request) {
            auto *state = new HeadlessAudioPathUpdateTask(runtime, request,
                                                         QCoreApplication::instance());
            auto result = state->prepare();
            if (!result)
                state->deleteLater();
            return result;
        }

        AutomationResult<QJsonObject> audioExportCapabilities(CoreRuntime &runtime,
                                                              const DocumentId &documentId) {
            const auto supported = AudioExportAutomationFacade::capabilities();
            const auto toJson = [](const auto &values) {
                QJsonArray result;
                for (const auto &value : values)
                    result.append(value);
                return result;
            };
            QJsonArray sources;
            auto project = runtime.project().getProject(documentId);
            if (!project)
                return project.getError();
            for (const auto &track : project.get().tracks) {
                sources.append(QJsonObject{
                    {QStringLiteral("id"),        track.id.value()                                            },
                    {QStringLiteral("name"),      track.data.name.isEmpty()
                                                 ? QStringLiteral("Track %1").arg(track.id.value())
                                                 : track.data.name},
                    {QStringLiteral("kind"),      QStringLiteral("track")                                     },
                    {QStringLiteral("available"), true                                                        },
                });
            }
            return QJsonObject{
                {QStringLiteral("formats"),       toJson(supported.formats)     },
                {QStringLiteral("sample_rates"),  toJson(supported.sampleRates) },
                {QStringLiteral("channel_modes"), toJson(supported.channelModes)},
                {QStringLiteral("mixing_modes"),  toJson(supported.mixingModes) },
                {QStringLiteral("source_modes"),  toJson(supported.sourceModes) },
                {QStringLiteral("sources"),       sources                       },
            };
        }

        AutomationResult<QJsonObject> extractionCapabilities(CoreRuntime &runtime,
                                                             SynthrtEngine *engine,
                                                             const DocumentId &documentId,
                                                             const ClipId clipId) {
            bool sourceSupported = false;
            bool sourceReady = false;
            QJsonArray languages;
            auto project = runtime.project().getProject(documentId);
            if (!project)
                return project.getError();
            bool sourceFound = false;
            for (const auto &track : project.get().tracks) {
                for (const auto &clip : track.clips) {
                    if (clip.id != clipId)
                        continue;
                    sourceFound = true;
                    if (clip.data.type != ClipDraftDto::Type::Audio) {
                        return AutomationError::wrongObjectType(
                            {ObjectKind::Clip, clipId.value()},
                            QStringLiteral("Extraction requires an audio clip"));
                    }
                    sourceSupported = true;
                    sourceReady = QFileInfo(clip.data.audioPath).isFile();
                    for (const auto &language : track.data.singerInfo.languages())
                        languages.append(language.id());
                    break;
                }
                if (sourceFound)
                    break;
            }
            if (!sourceFound) {
                return AutomationError::notFound({ObjectKind::Clip, clipId.value()},
                                                 QStringLiteral("Source audio clip was not found"));
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
                                         ? contract->inputSchema.value(QStringLiteral("properties"))
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
                    {QStringLiteral("model_id"),           id                                                                                          },
                    {QStringLiteral("display_name"),       name                                                                                        },
                    {QStringLiteral("configured"),         configured                                                                                  },
                    {QStringLiteral("available"),          available                                                                                   },
                    {QStringLiteral("unavailable_reason"), available    ? QString()
                                                           : configured ? moduleReason
                                                                        : configurationReason},
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
            return QJsonObject{
                {QStringLiteral("source_audio_clip_id"), clipId.value()},
                {QStringLiteral("pitch"),
                 QJsonObject{
                     {QStringLiteral("supported"), true},
                     {QStringLiteral("source_supported"), sourceSupported},
                     {QStringLiteral("available"),
                      sourceReady && pitchModuleReady && pitchConfigured},
                     {QStringLiteral("module_state"),
                      pitchModuleReady ? QStringLiteral("ready") : QStringLiteral("unavailable")},
                     {QStringLiteral("unavailable_reason"),
                      capabilityReason(sourceSupported, sourceReady, pitchModuleReady,
                                       pitchConfigured,
                                       QStringLiteral("Pitch extraction module is unavailable"),
                                       QStringLiteral("RMVPE model is not configured"))},
                     {QStringLiteral("models"),
                      QJsonArray{model(QStringLiteral("rmvpe"), QStringLiteral("RMVPE"),
                                       pitchConfigured, pitchModuleReady,
                                       QStringLiteral("RMVPE model is not configured"),
                                       QStringLiteral("Pitch extraction module is unavailable"))}},
                     {QStringLiteral("option_schema"),
                      optionSchema(OperationIds::extract::pitch::start)},
                     {QStringLiteral("range_support"),
                      QJsonObject{
                          {QStringLiteral("source_range"), QStringLiteral("visible_audio_clip")},
                          {QStringLiteral("custom_frequency"), false},
                      }},
                 }                                                     },
                {QStringLiteral("midi"),
                 QJsonObject{
                     {QStringLiteral("supported"), true},
                     {QStringLiteral("source_supported"), sourceSupported},
                     {QStringLiteral("available"),
                      sourceReady && midiModuleReady && midiConfigured},
                     {QStringLiteral("module_state"),
                      midiModuleReady ? QStringLiteral("ready") : QStringLiteral("unavailable")},
                     {QStringLiteral("unavailable_reason"),
                      capabilityReason(sourceSupported, sourceReady, midiModuleReady,
                                       midiConfigured,
                                       QStringLiteral("MIDI extraction module is unavailable"),
                                       QStringLiteral("GAME model directory is not configured"))},
                     {QStringLiteral("models"),
                      QJsonArray{model(QStringLiteral("game"), QStringLiteral("GAME"),
                                       midiConfigured, midiModuleReady,
                                       QStringLiteral("GAME model directory is not configured"),
                                       QStringLiteral("MIDI extraction module is unavailable"))}},
                     {QStringLiteral("option_schema"),
                      optionSchema(OperationIds::extract::midi::start)},
                     {QStringLiteral("range_support"),
                      QJsonObject{
                          {QStringLiteral("source_range"), QStringLiteral("visible_audio_clip")},
                          {QStringLiteral("minimum_note_length"),
                           QJsonObject{
                               {QStringLiteral("minimum"), 1},
                               {QStringLiteral("maximum"), std::numeric_limits<int>::max()},
                           }},
                      }},
                 }                                                     },
                {QStringLiteral("languages"),            languages     },
            };
        }

        AutomationResult<QList<InferPiece *>> inferencePieces(AppModel *model,
                                                              const QJsonObject &scope) {
            QList<InferPiece *> result;
            if (!model)
                return unavailable(QStringLiteral("Project model is unavailable"));
            QSet<int> trackIds;
            QSet<int> clipIds;
            for (const auto &value : scope.value(QStringLiteral("track_ids")).toArray())
                trackIds.insert(value.toInt());
            for (const auto &value : scope.value(QStringLiteral("clip_ids")).toArray())
                clipIds.insert(value.toInt());
            auto missingTrackIds = trackIds;
            auto missingClipIds = clipIds;
            const auto kind = scope.value(QStringLiteral("kind")).toString();
            for (auto *track : model->tracks()) {
                if (kind == QStringLiteral("track") && !trackIds.contains(track->id()))
                    continue;
                if (kind == QStringLiteral("track"))
                    missingTrackIds.remove(track->id());
                for (auto *clip : track->clips()) {
                    if (kind == QStringLiteral("clip") && !clipIds.contains(clip->id()))
                        continue;
                    if (kind == QStringLiteral("clip")) {
                        missingClipIds.remove(clip->id());
                        if (clip->clipType() != Clip::Singing) {
                            return AutomationError::wrongObjectType(
                                {ObjectKind::Clip, clip->id()},
                                QStringLiteral("Inference requires a singing clip"));
                        }
                    }
                    if (clip->clipType() != Clip::Singing)
                        continue;
                    result.append(static_cast<SingingClip *>(clip)->pieces());
                }
            }
            if (!missingTrackIds.isEmpty()) {
                return AutomationError::notFound(
                    {ObjectKind::Track, *missingTrackIds.cbegin()},
                    QStringLiteral("Inference scope track was not found"));
            }
            if (!missingClipIds.isEmpty()) {
                return AutomationError::notFound(
                    {ObjectKind::Clip, *missingClipIds.cbegin()},
                    QStringLiteral("Inference scope clip was not found"));
            }
            return result;
        }

        QString inferenceScopeKey(const QJsonObject &scope) {
            const auto kind = scope.value(QStringLiteral("kind")).toString();
            const auto idField = kind == QStringLiteral("track")  ? QStringLiteral("track_ids")
                                 : kind == QStringLiteral("clip") ? QStringLiteral("clip_ids")
                                                                  : QString();
            if (idField.isEmpty())
                return kind;
            QList<int> ids;
            for (const auto &value : scope.value(idField).toArray())
                ids.append(value.toInt());
            std::sort(ids.begin(), ids.end());
            ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
            QStringList encoded;
            for (const auto id : std::as_const(ids))
                encoded.append(QString::number(id));
            return kind + u':' + encoded.join(u',');
        }

        std::optional<AutomationTaskSnapshot> activeInferenceTask(CoreRuntime &runtime,
                                                                  const DocumentId &documentId,
                                                                  const QJsonObject &scope,
                                                                  const QString &stage) {
            const auto scopeKey = inferenceScopeKey(scope);
            for (const auto &task : runtime.automationTasks().list(documentId)) {
                if (task.operationId != OperationIds::inference::start)
                    continue;
                if (task.metadata.value(QStringLiteral("scope_key")).toString() != scopeKey)
                    continue;
                const auto stages = task.metadata.value(QStringLiteral("stages")).toArray();
                if (std::none_of(stages.cbegin(), stages.cend(), [&stage](const QJsonValue &value) {
                        return value.toString() == stage;
                    })) {
                    continue;
                }
                if (task.state == AutomationTaskState::Queued ||
                    task.state == AutomationTaskState::Running ||
                    task.state == AutomationTaskState::CancelRequested ||
                    task.state == AutomationTaskState::Committing) {
                    return task;
                }
            }
            return std::nullopt;
        }

        QString pieceInferenceStageState(const InferPiece &piece, const QString &stage,
                                         const bool hasActiveTask, QString *reason) {
            const auto stages = InferenceAutomationFacade::supportedStages();
            const auto targetIndex = stages.indexOf(stage);
            const auto state = piece.state.get();
            if (state == QStringLiteral("Ready"))
                return QStringLiteral("ready");
            if (state.isEmpty() || state == QStringLiteral("Unknown"))
                return QStringLiteral("idle");

            const auto prefix = state.section(u'.', 0, 0).toLower();
            const auto currentIndex = stages.indexOf(prefix);
            if (currentIndex < 0) {
                if (reason && reason->isEmpty())
                    *reason = state;
                return QStringLiteral("stale");
            }
            if (targetIndex < currentIndex)
                return QStringLiteral("ready");
            if (targetIndex > currentIndex)
                return hasActiveTask ? QStringLiteral("queued") : QStringLiteral("stale");
            if (state.endsWith(QStringLiteral(".Error")) ||
                state.endsWith(QStringLiteral(".Dropped"))) {
                if (reason && reason->isEmpty())
                    *reason = state;
                return QStringLiteral("failed");
            }
            return QStringLiteral("running");
        }

        AutomationResult<QJsonValue> inferenceStatus(CoreRuntime &runtime, AppModel *model,
                                                     const DocumentId &documentId,
                                                     const QJsonObject &scope) {
            auto project = runtime.project().getProject(documentId);
            if (!project)
                return project.getError();
            auto selected = inferencePieces(model, scope);
            if (!selected)
                return selected.getError();
            const QHash<QString, int> priorities{
                {QStringLiteral("ready"),   0},
                {QStringLiteral("idle"),    1},
                {QStringLiteral("stale"),   2},
                {QStringLiteral("queued"),  3},
                {QStringLiteral("running"), 4},
                {QStringLiteral("failed"),  5},
            };
            QJsonArray stages;
            for (const auto &stage : InferenceAutomationFacade::supportedStages()) {
                const auto activeTask = activeInferenceTask(runtime, documentId, scope, stage);
                QString aggregate =
                    selected.get().isEmpty() ? QStringLiteral("idle") : QStringLiteral("ready");
                QString reason;
                for (const auto *piece : selected.get()) {
                    if (!piece)
                        continue;
                    const auto state =
                        pieceInferenceStageState(*piece, stage, activeTask.has_value(), &reason);
                    if (priorities.value(state) > priorities.value(aggregate))
                        aggregate = state;
                }
                const auto exposesTask = activeTask && (aggregate == QStringLiteral("queued") ||
                                                        aggregate == QStringLiteral("running"));
                stages.append(QJsonObject{
                    {QStringLiteral("stage"),   stage                                                                            },
                    {QStringLiteral("state"),   aggregate                                                                        },
                    {QStringLiteral("reason"),  reason                                                                           },
                    {QStringLiteral("task_id"), exposesTask
                                                    ? QJsonValue(activeTask->taskId.toString())
                                                    : QJsonValue(QJsonValue::Null)},
                });
            }
            return AutomationResult<QJsonValue>(QJsonObject{
                {QStringLiteral("scope"),  scope },
                {QStringLiteral("stages"), stages},
            });
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
                return AutomationError::invalidArgument(
                    QStringLiteral("stage"), QStringLiteral("Inference stage is invalid"));
            }
            InferenceMutationRequest mutation;
            mutation.kind = InferenceMutationKind::ResetStage;
            mutation.stage = *stage;
            auto selected = inferencePieces(model, request.scope);
            if (!selected)
                return selected.getError();
            for (auto *piece : selected.get()) {
                mutation.pieceTargets.append({ClipId(piece->clipId()), PieceId(piece->id())});
            }
            if (mutation.pieceTargets.isEmpty()) {
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
            result.operationId = OperationIds::inference::start;
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
            [[nodiscard]] MutationResult completedMutation() const;

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

            auto selected = inferencePieces(m_model, m_request.scope);
            if (!selected)
                return selected.getError();
            if (selected.get().isEmpty()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("scope"),
                    QStringLiteral("The inference scope contains no singing pieces"));
            }
            for (auto *piece : selected.get()) {
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
            if (m_request.stages != supported.mid(earliest)) {
                return AutomationError::invalidArgument(
                    QStringLiteral("stages"),
                    QStringLiteral("Inference stages must be a contiguous pipeline suffix"));
            }
            m_earliestStage = supported.at(earliest);

            auto settings = m_runtime.settings().getSettings();
            if (!settings)
                return settings.getError();
            const auto &active = settings.get().inference;
            const auto provider = m_request.options.value(QStringLiteral("provider_id")).toString();
            if (!provider.isEmpty() && provider != active.executionProvider) {
                return AutomationError::invalidArgument(
                    QStringLiteral("options.provider_id"),
                    QStringLiteral("The requested inference provider is not active"));
            }
            const auto device = m_request.options.value(QStringLiteral("device_id")).toString();
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
            QJsonArray taskStages;
            for (const auto &stage : std::as_const(m_request.stages))
                taskStages.append(stage);
            const auto task = m_runtime.automationTasks().createTask(
                OperationIds::inference::start, base.get(), std::nullopt,
                [weak] {
                    if (weak)
                        weak->requestCancel();
            },
                m_request.command.clientId,
                QJsonObject{
                    {QStringLiteral("scope_key"), inferenceScopeKey(m_request.scope)},
                    {QStringLiteral("stages"), taskStages},
                });
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
            auto reset = resetInference(m_runtime, m_model,
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
            m_mutation = completedMutation();
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
            error.operationId = OperationIds::inference::start;
            error.taskId = m_taskId;
            for (const auto &piece : std::as_const(m_pieces)) {
                if (piece)
                    inferController->cancelPieceInference(piece->id());
            }
            m_runtime.automationTasks().fail(m_taskId, std::move(error));
            deleteLater();
        }

        MutationResult HeadlessInferenceTask::completedMutation() const {
            auto result = m_mutation;
            result.current = m_runtime.documentVersion();
            result.changed = result.changed || result.current != result.previous;
            return result;
        }

        AutomationResult<TaskAcceptedResult> startInference(CoreRuntime &runtime, AppModel *model,
                                                            PublicInferenceStartRequest request) {
            auto *state = new HeadlessInferenceTask(runtime, model, std::move(request),
                                                    QCoreApplication::instance());
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
                                    request.encoding.toLatin1(), request.importTempo,
                                    request.importTimeSignature, request.command,
                                    request.revalidatePlan);
        };
        services.importDocument = [&runtime](const PublicDocumentImportRequest &request) {
            return startProjectLoad(runtime, request.canonicalPath, request.mergeMode,
                                    ProjectLoadPurpose::Import, request.encoding.toLatin1(),
                                    request.importTempo, request.importTimeSignature,
                                    request.command, request.revalidatePlan);
        };
        services.importDocuments = [&runtime,
                                    model](const PublicDocumentBatchImportRequest &request) {
            return startProjectBatchLoad(runtime, model, request);
        };
        services.importAudioClip = [&runtime, model](const PublicAudioClipImportRequest &request) {
            return startAudioImport(
                runtime, model, request.command,
                {
                    {request.trackId,
                     audioClipDraft(request.canonicalPath, request.properties, request.clientRef)}
            },
                {}, PublicBatchFailurePolicy::Atomic,
                OperationIds::audio_clips::import_audio);
        };
        services.importAudioClips = [&runtime,
                                     model](const PublicAudioClipBatchImportRequest &request) {
            QList<ClipInsertDto> clips;
            QStringList validationErrors;
            clips.reserve(request.items.size());
            validationErrors.reserve(request.items.size());
            for (const auto &item : request.items) {
                clips.append({item.trackId,
                              audioClipDraft(item.canonicalPath, item.properties, item.clientRef)});
                validationErrors.append(item.validationError ? item.validationError->message
                                                             : QString());
            }
            return startAudioImport(runtime, model, request.command, std::move(clips),
                                    std::move(validationErrors), request.failurePolicy,
                                    OperationIds::audio_clips::import_batch);
        };
        services.updateAudioClipPath = [&runtime](const PublicAudioPathUpdateRequest &request) {
            return startAudioPathUpdate(runtime, request);
        };
        services.audioExportCapabilities = [&runtime](const DocumentId &documentId) {
            auto capabilities = audioExportCapabilities(runtime, documentId);
            if (!capabilities)
                return AutomationResult<QJsonValue>(capabilities.getError());
            return AutomationResult<QJsonValue>(capabilities.get());
        };
        services.extractionCapabilities = [&runtime, synthrtEngine](const DocumentId &documentId,
                                                                    const ClipId clipId) {
            auto capabilities = extractionCapabilities(runtime, synthrtEngine, documentId, clipId);
            if (!capabilities)
                return AutomationResult<QJsonValue>(capabilities.getError());
            return AutomationResult<QJsonValue>(capabilities.get());
        };
        services.inferenceCapabilities = [&runtime, model](const DocumentId &documentId,
                                                           const QJsonObject &scope) {
            auto project = runtime.project().getProject(documentId);
            if (!project)
                return AutomationResult<QJsonValue>(project.getError());
            auto pieces = inferencePieces(model, scope);
            if (!pieces)
                return AutomationResult<QJsonValue>(pieces.getError());
            QJsonArray stages;
            for (const auto &stage : InferenceAutomationFacade::supportedStages())
                stages.append(stage);
            auto settings = runtime.settings().getSettings();
            QJsonArray providers;
            QJsonArray devices;
            if (settings) {
                const auto &inference = settings.get().inference;
                if (!inference.executionProvider.isEmpty()) {
                    providers.append(QJsonObject{
                        {QStringLiteral("id"),                 inference.executionProvider},
                        {QStringLiteral("display_name"),       inference.executionProvider},
                        {QStringLiteral("available"),          true                       },
                        {QStringLiteral("unavailable_reason"), QString()                  },
                    });
                }
                if (!inference.selectedGpuId.isEmpty()) {
                    devices.append(QJsonObject{
                        {QStringLiteral("id"),                 inference.selectedGpuId},
                        {QStringLiteral("display_name"),       inference.selectedGpuId},
                        {QStringLiteral("available"),          true                   },
                        {QStringLiteral("unavailable_reason"), QString()              },
                    });
                }
            }
            QJsonArray models;
            QSet<QString> modelIds;
            const auto engineReady = inferEngine->initialized();
            for (const auto *piece : pieces.get()) {
                const auto &identifier = piece->identifier;
                const auto id = QStringLiteral("%1/%2@%3")
                                    .arg(identifier.packageId, identifier.singerId,
                                         identifier.packageVersion.toString());
                if (identifier.isEmpty() || modelIds.contains(id))
                    continue;
                modelIds.insert(id);
                models.append(QJsonObject{
                    {QStringLiteral("model_id"),           id                               },
                    {QStringLiteral("display_name"),       identifier.singerId              },
                    {QStringLiteral("available"),          engineReady                      },
                    {QStringLiteral("unavailable_reason"),
                     engineReady ? QString()
                                 : QStringLiteral("The inference engine is not initialized")},
                });
            }
            return AutomationResult<QJsonValue>(QJsonObject{
                {QStringLiteral("scope"),     scope    },
                {QStringLiteral("stages"),    stages   },
                {QStringLiteral("providers"), providers},
                {QStringLiteral("devices"),   devices  },
                {QStringLiteral("models"),    models   },
            });
        };
        services.inferenceStatus = [&runtime, model](const DocumentId &documentId,
                                                     const QJsonObject &scope) {
            return inferenceStatus(runtime, model, documentId, scope);
        };
        services.resetInferenceStage = [&runtime,
                                        model](const PublicInferenceResetRequest &request) {
            return resetInference(runtime, model, request);
        };
        services.startInference = [&runtime, model](const PublicInferenceStartRequest &request) {
            return startInference(runtime, model, request);
        };
        return services;
    }

} // namespace Automation
