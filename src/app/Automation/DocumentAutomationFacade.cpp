#include "DocumentAutomationFacade.h"

#include "Controller/Actions/AppModel/ImportProjectActions.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QCryptographicHash>
#include <QFileInfo>

#include <memory>

namespace Automation {
    namespace {
        QByteArray replaceFingerprint(const DocumentDraftDto &document, const QString &path,
                                      const QString &projectName, const bool savedBaseline) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(fingerprint(document));
            hash.addData("\0", 1);
            hash.addData(path.toUtf8());
            hash.addData("\0", 1);
            hash.addData(projectName.toUtf8());
            hash.addData(savedBaseline ? "1" : "0", 1);
            return hash.result();
        }

        QByteArray importFingerprint(const DocumentDraftDto &document, const bool importTempo,
                                     const bool importTimeSignature) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(fingerprint(document));
            hash.addData(importTempo ? "1" : "0", 1);
            hash.addData(importTimeSignature ? "1" : "0", 1);
            return hash.result();
        }

        AutomationError missingRuntime(const QString &message) {
            AutomationError error;
            error.code = AutomationErrorCode::HostCapabilityUnavailable;
            error.message = message;
            return error;
        }
    }

    DocumentAutomationFacade::DocumentAutomationFacade(OperationCatalog &catalog,
                                                       AutomationDispatcher &dispatcher,
                                                       CommandCommitter &committer,
                                                       AutomationTaskManager &tasks,
                                                       DocumentRuntimeServices services)
        : m_catalog(catalog), m_dispatcher(dispatcher), m_committer(committer), m_tasks(tasks),
          m_services(std::move(services)) {
        registerOperations();
    }

    AutomationResult<DocumentSnapshotDto>
        DocumentAutomationFacade::getDocument(const DocumentId &documentId) {
        return m_dispatcher.dispatchDocumentQuery<DocumentSnapshotDto>(
            QStringLiteral("documents.get"), documentId, [](DocumentSession &session) {
                auto *history = session.history();
                return AutomationResult<DocumentSnapshotDto>({
                    .document = session.version(),
                    .path = session.path(),
                    .projectName = session.projectName(),
                    .lifecycle = session.lifecycleState(),
                    .busy = session.isBusy(),
                    .saved = !history || history->isOnSavePoint(),
                });
            });
    }

    AutomationResult<MutationResult>
        DocumentAutomationFacade::commitNewDocument(const CommandContext &context,
                                                    const DocumentDraftDto &document) {
        return replaceDocument(QStringLiteral("documents.commit_new"), context, document, {}, {},
                               true);
    }

    AutomationResult<MutationResult> DocumentAutomationFacade::commitOpenedDocument(
        const CommandContext &context, const DocumentDraftDto &document, const QString &path,
        const QString &projectName, const bool savedBaseline) {
        return replaceDocument(QStringLiteral("documents.commit_open"), context, document, path,
                               projectName, savedBaseline);
    }

    AutomationResult<MutationResult> DocumentAutomationFacade::replaceDocument(
        const OperationId &operationId, const CommandContext &context,
        const DocumentDraftDto &document, const QString &path, const QString &projectName,
        const bool savedBaseline) {
        return m_dispatcher.dispatchDocumentCommand(
            operationId, context, replaceFingerprint(document, path, projectName, savedBaseline),
            [this, document, path, projectName, savedBaseline](DocumentSession &session,
                                                               const bool validateOnly) {
                auto validation = validate(document);
                if (!validation)
                    return AutomationResult<MutationResult>(validation.getError());
                if (validateOnly) {
                    MutationResult preview;
                    preview.previous = session.version();
                    preview.current = {DocumentId(), 0};
                    preview.changed = true;
                    preview.validatedOnly = true;
                    return AutomationResult<MutationResult>(std::move(preview));
                }
                QList<CreatedObjectRef> createdObjects;
                auto prepared = buildProjectModelData(document, &createdObjects);
                auto *model = session.model();
                auto *history = session.history();
                if (!model || !history)
                    return AutomationResult<MutationResult>(
                        missingRuntime(QStringLiteral("Document runtime is unavailable")));

                MutationResult result;
                result.previous = session.version();
                session.setLifecycleState(DocumentLifecycleState::Replacing);
                if (m_services.beforeReplaceGeneration)
                    m_services.beforeReplaceGeneration(result.previous.documentId);
                m_tasks.discardDocumentGeneration(result.previous.documentId);
                history->reset(HistoryManager::ResetState::Saved);
                model->replaceProject(std::move(prepared));
                if (m_services.applyLoopSettings)
                    m_services.applyLoopSettings(document.loopSettings);
                history->reset(savedBaseline ? HistoryManager::ResetState::Saved
                                             : HistoryManager::ResetState::Unsaved);
                result.current = session.replaceGeneration(path, projectName);
                result.changed = true;
                result.createdObjects = std::move(createdObjects);
                return AutomationResult<MutationResult>(std::move(result));
            });
    }

    AutomationResult<MutationResult> DocumentAutomationFacade::commitImportedDocument(
        const CommandContext &context, const DocumentDraftDto &document, const bool importTempo,
        const bool importTimeSignature) {
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("documents.commit_import"), context,
            importFingerprint(document, importTempo, importTimeSignature),
            [this, document, importTempo, importTimeSignature](DocumentSession &session,
                                                               const bool validateOnly) {
                auto validation = validate(document);
                if (!validation)
                    return AutomationResult<MutationResult>(validation.getError());
                auto *model = session.model();
                if (!model)
                    return AutomationResult<MutationResult>(
                        missingRuntime(QStringLiteral("Document model is unavailable")));
                const bool timelineChanged =
                    (importTempo && model->timeline().tempos() != document.timeline.tempos()) ||
                    (importTimeSignature &&
                     model->timeline().timeSignatures() != document.timeline.timeSignatures());
                const bool changed = timelineChanged || !document.tracks.isEmpty();
                if (validateOnly)
                    return AutomationResult<MutationResult>(m_committer.preview(session, changed));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));

                QList<CreatedObjectRef> createdObjects;
                auto prepared = buildProjectModelData(document, &createdObjects);
                QList<ObjectRef> affected;
                affected.reserve(static_cast<qsizetype>(prepared.tracks.size()));
                for (const auto &track : prepared.tracks) {
                    if (track)
                        affected.append({ObjectKind::Track, track->id()});
                }
                auto actions = std::make_unique<ImportProjectActions>(
                    std::move(prepared), importTempo, importTimeSignature, model);
                return m_committer.commit(session, std::move(actions), affected,
                                          std::move(createdObjects));
            });
    }

    AutomationResult<MutationResult>
        DocumentAutomationFacade::saveDocument(const CommandContext &context, const QString &path) {
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("documents.save"), context, path.toUtf8(),
            [this, path](DocumentSession &session, const bool validateOnly) {
                if (path.trimmed().isEmpty()) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("path"), QStringLiteral("Save path is empty")));
                }
                if (!m_services.saveProject) {
                    return AutomationResult<MutationResult>(
                        missingRuntime(QStringLiteral("Project saving is unavailable")));
                }
                MutationResult result;
                result.previous = session.version();
                result.current = result.previous;
                result.changed = true;
                result.validatedOnly = validateOnly;
                if (validateOnly)
                    return AutomationResult<MutationResult>(std::move(result));

                QString errorMessage;
                if (!m_services.saveProject(path, session.model(), errorMessage)) {
                    AutomationError error;
                    error.code = AutomationErrorCode::IoError;
                    error.fieldPath = QStringLiteral("path");
                    error.message = errorMessage;
                    return AutomationResult<MutationResult>(std::move(error));
                }
                if (auto *history = session.history())
                    history->setSavePoint();
                session.setPathAndProjectName(path, QFileInfo(path).fileName());
                return AutomationResult<MutationResult>(std::move(result));
            });
    }

    void DocumentAutomationFacade::registerOperations() {
        const auto add = [this](OperationDescriptor descriptor) {
            const auto result = m_catalog.add(std::move(descriptor));
            Q_ASSERT(result);
        };
        add({
            .id = QStringLiteral("documents.get"),
            .category = QStringLiteral("documents"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .inputContract = QStringLiteral("automation.DocumentRef.v1"),
            .outputContract = QStringLiteral("automation.DocumentSnapshot.v1"),
            .documentPolicy = DocumentPolicy::Read,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
        const auto addReplace = [&add](const QString &id) {
            add({
                .id = id,
                .category = QStringLiteral("documents"),
                .kind = OperationKind::Command,
                .syncMode = SyncMode::Synchronous,
                .inputContract = QStringLiteral("automation.PreparedDocumentCommand.v1"),
                .outputContract = QStringLiteral("automation.MutationResult.v1"),
                .documentPolicy = DocumentPolicy::Replace,
                .revisionPolicy = RevisionPolicy::Reset,
                .historyPolicy = HistoryPolicy::None,
                .fileAccess = FileAccessPolicy::None,
                .hostAvailability = HostAvailability::Core,
                .safety = SafetyClass::Destructive,
                .exposure = ExposurePolicy::InternalOnly,
                .idempotency = IdempotencyPolicy::Unsupported,
            });
        };
        addReplace(QStringLiteral("documents.commit_new"));
        addReplace(QStringLiteral("documents.commit_open"));
        add({
            .id = QStringLiteral("documents.commit_import"),
            .category = QStringLiteral("documents"),
            .kind = OperationKind::Command,
            .syncMode = SyncMode::Synchronous,
            .inputContract = QStringLiteral("automation.PreparedImportCommand.v1"),
            .outputContract = QStringLiteral("automation.MutationResult.v1"),
            .documentPolicy = DocumentPolicy::Write,
            .revisionPolicy = RevisionPolicy::Increment,
            .historyPolicy = HistoryPolicy::Record,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::Reversible,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::DocumentGeneration,
        });
        add({
            .id = QStringLiteral("documents.save"),
            .category = QStringLiteral("documents"),
            .kind = OperationKind::Command,
            .syncMode = SyncMode::Synchronous,
            .inputContract = QStringLiteral("automation.SaveDocumentCommand.v1"),
            .outputContract = QStringLiteral("automation.MutationResult.v1"),
            .documentPolicy = DocumentPolicy::Write,
            .revisionPolicy = RevisionPolicy::Check,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::Write,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::FileSystem,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::DocumentGeneration,
        });
    }

} // namespace Automation
