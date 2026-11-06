#ifndef DOCUMENTSESSION_H
#define DOCUMENTSESSION_H

#include "AutomationTypes.h"
#include "IdempotencyStore.h"

class AppModel;
class HistoryManager;

namespace Automation {

    enum class DocumentLifecycleState {
        Active,
        Replacing,
        Closing,
    };

    class DocumentSession final {
    public:
        DocumentSession(AppModel *model, HistoryManager *historyManager);

        [[nodiscard]] DocumentVersion version() const;
        [[nodiscard]] const DocumentId &documentId() const;
        [[nodiscard]] Revision revision() const;
        [[nodiscard]] AppModel *model() const;
        [[nodiscard]] HistoryManager *history() const;
        [[nodiscard]] const QString &path() const;
        [[nodiscard]] const QString &projectName() const;
        [[nodiscard]] DocumentLifecycleState lifecycleState() const;
        [[nodiscard]] bool isBusy() const;

        void setPathAndProjectName(QString path, QString projectName);
        void setLifecycleState(DocumentLifecycleState state);
        void setBusy(bool busy);

        // Commits the document to Closing. Irreversible: later lifecycle updates from
        // an operation that was already in flight are ignored, so a replacement race
        // cannot reopen a document that is on its way out.
        void commitClosing();

        DocumentVersion replaceGeneration(QString path, QString projectName);
        DocumentVersion advanceRevision();

        IdempotencyStore &idempotencyStore();
        const IdempotencyStore &idempotencyStore() const;

    private:
        AppModel *m_model;
        HistoryManager *m_historyManager;
        DocumentId m_documentId;
        Revision m_revision = 0;
        QString m_path;
        QString m_projectName;
        DocumentLifecycleState m_lifecycleState = DocumentLifecycleState::Active;
        bool m_busy = false;
        bool m_closingCommitted = false;
        IdempotencyStore m_idempotencyStore;
    };

} // namespace Automation

#endif // DOCUMENTSESSION_H
