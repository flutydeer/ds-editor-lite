#ifndef DOCUMENTSESSION_H
#define DOCUMENTSESSION_H

#include "AutomationTypes.h"
#include "IdempotencyStore.h"
#include <functional>

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
        DocumentSession(AppModel *model, HistoryManager *historyManager,
                        std::function<bool()> hasPendingDialog = {});

        [[nodiscard]] DocumentVersion version() const;
        [[nodiscard]] const DocumentId &documentId() const;
        [[nodiscard]] Revision revision() const;
        [[nodiscard]] AppModel *model() const;
        [[nodiscard]] HistoryManager *history() const;
        [[nodiscard]] const QString &path() const;
        [[nodiscard]] const QString &projectName() const;
        [[nodiscard]] DocumentLifecycleState lifecycleState() const;
        [[nodiscard]] bool isBusy() const;
        [[nodiscard]] bool hasPendingDialog() const;

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
        std::function<bool()> m_hasPendingDialog;
        bool m_closingCommitted = false;
        IdempotencyStore m_idempotencyStore;
    };

} // namespace Automation

#endif // DOCUMENTSESSION_H
