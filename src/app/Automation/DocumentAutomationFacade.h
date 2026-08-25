#ifndef DOCUMENTAUTOMATIONFACADE_H
#define DOCUMENTAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"
#include "AutomationTaskManager.h"
#include "CommandCommitter.h"
#include "ProjectAutomationDtos.h"

#include <functional>

class AppModel;

namespace Automation {

    struct DocumentRuntimeServices {
        std::function<void(const LoopSettings &)> applyLoopSettings;
        std::function<bool(const QString &, AppModel *, QString &)> saveProject;
        std::function<void(const DocumentId &)> beforeReplaceGeneration;
    };

    struct DocumentSnapshotDto {
        DocumentVersion document;
        QString path;
        QString projectName;
        DocumentLifecycleState lifecycle = DocumentLifecycleState::Active;
        bool busy = false;
        bool saved = true;
    };

    class DocumentAutomationFacade final {
    public:
        DocumentAutomationFacade(OperationCatalog &catalog,
                                 AutomationDispatcher &dispatcher,
                                 CommandCommitter &committer,
                                 AutomationTaskManager &tasks,
                                 DocumentRuntimeServices services = {});

        AutomationResult<DocumentSnapshotDto> getDocument(const DocumentId &documentId);
        static DocumentDraftDto newDocumentDraft(bool defaultTemplate);
        AutomationResult<MutationResult> commitNewDocument(const CommandContext &context,
                                                           const DocumentDraftDto &document);
        AutomationResult<MutationResult> commitOpenedDocument(
            const CommandContext &context,
            const DocumentDraftDto &document,
            const QString &path,
            const QString &projectName,
            bool savedBaseline);
        AutomationResult<MutationResult> commitImportedDocument(
            const CommandContext &context,
            const DocumentDraftDto &document,
            bool importTempo,
            bool importTimeSignature);
        AutomationResult<MutationResult> saveDocument(const CommandContext &context,
                                                      const QString &path,
                                                      bool allowOverwrite = true);
        AutomationResult<MutationResult> saveDocumentAs(const CommandContext &context,
                                                        const QString &path,
                                                        bool allowOverwrite = true);

    private:
        AutomationResult<MutationResult> replaceDocument(
            const OperationId &operationId,
            const CommandContext &context,
            const DocumentDraftDto &document,
            const QString &path,
            const QString &projectName,
            bool savedBaseline);
        AutomationResult<MutationResult> saveDocumentWithOperation(
            const OperationId &operationId,
            const CommandContext &context,
            const QString &path,
            bool allowOverwrite);
        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        CommandCommitter &m_committer;
        AutomationTaskManager &m_tasks;
        DocumentRuntimeServices m_services;
    };

} // namespace Automation

#endif // DOCUMENTAUTOMATIONFACADE_H
