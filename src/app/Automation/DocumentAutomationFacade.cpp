#include "DocumentAutomationFacade.h"
#include "OperationIds.h"

#include "Controller/Actions/AppModel/ImportProjectActions.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QTemporaryFile>

#include <memory>

namespace Automation {
    namespace {
        AutomationError missingRuntime(const QString &message) {
            AutomationError error;
            error.code = AutomationErrorCode::HostCapabilityUnavailable;
            error.message = message;
            return error;
        }
    }

    DocumentAutomationFacade::DocumentAutomationFacade(AutomationDispatcher &dispatcher,
                                                       CommandCommitter &committer,
                                                       AutomationTaskManager &tasks,
                                                       DocumentRuntimeServices services)
        : m_dispatcher(dispatcher), m_committer(committer), m_tasks(tasks),
          m_services(std::move(services)) {
    }

    AutomationResult<DocumentSnapshotDto>
        DocumentAutomationFacade::getDocument(const DocumentId &documentId) {
        return m_dispatcher.dispatchDocumentQuery<DocumentSnapshotDto>(
            OperationIds::documents::get, documentId, [](DocumentSession &session) {
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

    DocumentDraftDto DocumentAutomationFacade::newDocumentDraft(const bool defaultTemplate) {
        AppModel model;
        if (defaultTemplate)
            model.newProject();
        return documentDraftDto(model.takeProjectData(), {});
    }

    AutomationResult<MutationResult>
        DocumentAutomationFacade::commitNewDocument(const CommandContext &context,
                                                    const DocumentDraftDto &document) {
        return replaceDocument(OperationIds::documents::commit_new, context, document, {}, {},
                               true);
    }

    AutomationResult<MutationResult> DocumentAutomationFacade::commitOpenedDocument(
        const CommandContext &context, const DocumentDraftDto &document, const QString &path,
        const QString &projectName, const bool savedBaseline) {
        return replaceDocument(OperationIds::documents::commit_open, context, document, path,
                               projectName, savedBaseline);
    }

    AutomationResult<MutationResult> DocumentAutomationFacade::replaceDocument(
        const OperationId &operationId, const CommandContext &context,
        const DocumentDraftDto &document, const QString &path, const QString &projectName,
        const bool savedBaseline) {
        return m_dispatcher.dispatchDocumentCommand(
            operationId, context,
            [this, document, path, projectName, savedBaseline,
             taskId = context.taskId](DocumentSession &session, const bool validateOnly) {
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
                history->reset(HistoryManager::ResetState::Saved);
                model->replaceProject(std::move(prepared));
                if (m_services.applyLoopSettings)
                    m_services.applyLoopSettings(document.loopSettings);
                history->reset(savedBaseline ? HistoryManager::ResetState::Saved
                                             : HistoryManager::ResetState::Unsaved);
                result.current = session.replaceGeneration(path, projectName);
                m_tasks.replaceDocumentGeneration(result.previous.documentId, result.current,
                                                  taskId);
                result.changed = true;
                result.createdObjects = std::move(createdObjects);
                result.presentationEffects.append(QStringLiteral("active_document_changed"));
                return AutomationResult<MutationResult>(std::move(result));
            });
    }

    AutomationResult<MutationResult> DocumentAutomationFacade::commitImportedDocument(
        const CommandContext &context, const DocumentDraftDto &document, const bool importTempo,
        const bool importTimeSignature) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::documents::commit_import, context,
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
        DocumentAutomationFacade::saveDocument(const CommandContext &context, const QString &path,
                                               const bool allowOverwrite) {
        return saveDocumentWithOperation(OperationIds::documents::save, context, path,
                                         allowOverwrite);
    }

    AutomationResult<MutationResult>
        DocumentAutomationFacade::saveDocumentAs(const CommandContext &context, const QString &path,
                                                 const bool allowOverwrite) {
        return saveDocumentWithOperation(OperationIds::documents::save_as, context, path,
                                         allowOverwrite);
    }

    AutomationResult<MutationResult> DocumentAutomationFacade::saveDocumentWithOperation(
        const OperationId &operationId, const CommandContext &context, const QString &path,
        const bool allowOverwrite) {
        return m_dispatcher.dispatchDocumentCommand(
            operationId, context,
            [this, path, allowOverwrite](DocumentSession &session, const bool validateOnly) {
                if (path.trimmed().isEmpty()) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("path"), QStringLiteral("Save path is empty")));
                }
                if (!m_services.saveProject) {
                    return AutomationResult<MutationResult>(
                        missingRuntime(QStringLiteral("Project saving is unavailable")));
                }
                if (!allowOverwrite && QFileInfo::exists(path)) {
                    AutomationError error;
                    error.code = AutomationErrorCode::OverwriteDenied;
                    error.fieldPath = QStringLiteral("allow_overwrite");
                    error.message = QStringLiteral("The destination file already exists");
                    return AutomationResult<MutationResult>(std::move(error));
                }
                MutationResult result;
                result.previous = session.version();
                result.current = result.previous;
                result.changed = true;
                result.validatedOnly = validateOnly;
                if (validateOnly)
                    return AutomationResult<MutationResult>(std::move(result));

                QString errorMessage;
                QString savePath = path;
                if (!allowOverwrite) {
                    QTemporaryFile stagingFile(
                        QDir(QFileInfo(path).absolutePath())
                            .filePath(QStringLiteral(".ds-editor-lite-document-XXXXXX.dspx")));
                    if (!stagingFile.open()) {
                        AutomationError error;
                        error.code = AutomationErrorCode::IoError;
                        error.fieldPath = QStringLiteral("path");
                        error.message =
                            QStringLiteral("Project save staging file could not be created");
                        return AutomationResult<MutationResult>(std::move(error));
                    }
                    savePath = stagingFile.fileName();
                    stagingFile.close();
                    stagingFile.setAutoRemove(false);
                }
                auto removeStaging = qScopeGuard([&] {
                    if (!allowOverwrite)
                        QFile::remove(savePath);
                });
                if (!m_services.saveProject(savePath, session.model(), errorMessage)) {
                    AutomationError error;
                    error.code = AutomationErrorCode::IoError;
                    error.fieldPath = QStringLiteral("path");
                    error.message = errorMessage;
                    return AutomationResult<MutationResult>(std::move(error));
                }
                if (!allowOverwrite && !QFile(savePath).rename(path)) {
                    AutomationError error;
                    error.code = QFileInfo::exists(path) ? AutomationErrorCode::OverwriteDenied
                                                         : AutomationErrorCode::IoError;
                    error.fieldPath = error.code == AutomationErrorCode::OverwriteDenied
                                          ? QStringLiteral("allow_overwrite")
                                          : QStringLiteral("path");
                    error.message = error.code == AutomationErrorCode::OverwriteDenied
                                        ? QStringLiteral("The destination file already exists")
                                        : QStringLiteral("Saved project could not be published");
                    return AutomationResult<MutationResult>(std::move(error));
                }
                if (!allowOverwrite)
                    removeStaging.dismiss();
                if (auto *history = session.history())
                    history->setSavePoint();
                session.setPathAndProjectName(path, QFileInfo(path).fileName());
                return AutomationResult<MutationResult>(std::move(result));
            });
    }

} // namespace Automation
