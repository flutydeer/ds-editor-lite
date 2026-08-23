#include "DocumentSession.h"

namespace Automation {

    DocumentSession::DocumentSession(AppModel *model, HistoryManager *historyManager)
        : m_model(model), m_historyManager(historyManager), m_documentId(DocumentId::create()) {
    }

    DocumentVersion DocumentSession::version() const {
        return {m_documentId, m_revision};
    }

    const DocumentId &DocumentSession::documentId() const {
        return m_documentId;
    }

    Revision DocumentSession::revision() const {
        return m_revision;
    }

    AppModel *DocumentSession::model() const {
        return m_model;
    }

    HistoryManager *DocumentSession::history() const {
        return m_historyManager;
    }

    const QString &DocumentSession::path() const {
        return m_path;
    }

    const QString &DocumentSession::projectName() const {
        return m_projectName;
    }

    DocumentLifecycleState DocumentSession::lifecycleState() const {
        return m_lifecycleState;
    }

    bool DocumentSession::isBusy() const {
        return m_busy;
    }

    void DocumentSession::setPathAndProjectName(QString path, QString projectName) {
        m_path = std::move(path);
        m_projectName = std::move(projectName);
    }

    void DocumentSession::setLifecycleState(const DocumentLifecycleState state) {
        m_lifecycleState = state;
    }

    void DocumentSession::setBusy(const bool busy) {
        m_busy = busy;
    }

    DocumentVersion DocumentSession::replaceGeneration(QString path, QString projectName) {
        m_documentId = DocumentId::create();
        m_revision = 0;
        m_path = std::move(path);
        m_projectName = std::move(projectName);
        m_lifecycleState = DocumentLifecycleState::Active;
        m_busy = false;
        m_idempotencyStore.clear();
        return version();
    }

    DocumentVersion DocumentSession::advanceRevision() {
        ++m_revision;
        return version();
    }

    IdempotencyStore &DocumentSession::idempotencyStore() {
        return m_idempotencyStore;
    }

    const IdempotencyStore &DocumentSession::idempotencyStore() const {
        return m_idempotencyStore;
    }

} // namespace Automation
