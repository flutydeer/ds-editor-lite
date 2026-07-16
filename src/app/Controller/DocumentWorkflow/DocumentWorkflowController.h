#ifndef DOCUMENTWORKFLOWCONTROLLER_H
#define DOCUMENTWORKFLOWCONTROLLER_H

#define documentWorkflowController DocumentWorkflowController::instance()

#include "ProjectLoadTypes.h"
#include "Utils/Singleton.h"

#include <QObject>
#include <QStringList>

#include <optional>

class IDocumentWorkflowUi;
class IProjectLoadSession;
class ProgressDialog;
class QState;
class QStateMachine;
class Track;

enum class DocumentOperation { New, Open, Import, Save, SaveAs };
enum class TerminationMode { Exit, Restart };

class DocumentWorkflowController final : public QObject {
    Q_OBJECT

private:
    explicit DocumentWorkflowController(QObject *parent = nullptr);
    ~DocumentWorkflowController() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(DocumentWorkflowController)
    Q_DISABLE_COPY_MOVE(DocumentWorkflowController)

    void setUi(IDocumentWorkflowUi *ui);
    void initializeNewDocument();

    void requestNew();
    void requestOpen(const QString &path);
    void requestImport(const QString &path);
    void requestSave();
    void requestSaveAs();
    void requestTermination(TerminationMode mode);
    void cancelCurrentOperation();

    [[nodiscard]] bool busy() const;
    [[nodiscard]] QString projectPath() const;
    [[nodiscard]] QString projectName() const;
    [[nodiscard]] QString lastProjectFolder() const;
    [[nodiscard]] QStringList recentProjectFiles() const;
    void clearRecentProjectFiles();
    void removeRecentProjectFile(const QString &filePath);

signals:
    void busyChanged(bool busy);
    void documentIdentityChanged();
    void recentProjectFilesChanged(const QStringList &filePaths);
    void terminationApproved(TerminationMode mode);

    void beginRequested();
    void validationCompleted();
    void saveDecisionCompleted();
    void savePathSelectionCompleted();
    void saveCompleted();
    void operationFailed();
    void sessionStarted();
    void sessionReadyEvent();
    void sessionFailedEvent();
    void sessionCanceledEvent();
    void cancelSessionRequested();
    void commitFinished();
    void failureHandled();

private:
    enum class ValidationResult {
        AwaitSaveDecision,
        AwaitSavePath,
        Save,
        StartSession,
        Commit,
        Finish,
        Fail,
    };
    enum class SaveDecisionResult { SaveWithPath, SaveWithoutPath, Discard, Cancel };
    enum class SavePathResult { Selected, Canceled };
    enum class SaveResult { SucceededAndResume, SucceededAndFinish, FailedAndResume, Failed };

    struct PendingRequest {
        quint64 requestId = 0;
        std::optional<DocumentOperation> operation;
        std::optional<TerminationMode> termination;
        QString filePath;
    };

    void initializeStateMachine();
    void begin(DocumentOperation operation, const QString &filePath = {});
    void validatePendingRequest();
    void askSaveDecision();
    void askSavePath();
    void performSave();
    void createSession();
    void startSession();
    void commitPreparedProject();
    void enterIdle();
    void enterFailed();
    void cleanSession();
    void ensureProgressDialog(const ProjectLoadProgress &progress);
    void closeProgressDialog();
    void handleSessionReady(IProjectLoadSession *session);
    void handleSessionFailed(IProjectLoadSession *session, const ProjectOperationError &error);
    void handleSessionCanceled(IProjectLoadSession *session);
    void prepareNewProject();
    void commitReplace(ReplaceProjectPayload &&payload);
    void commitAppend(AppendProjectPayload &&payload);
    void activateFirstClip(const QList<Track *> &preferredTracks = {});
    void updateProjectIdentity(const QString &path, const QString &name = {});
    void addRecentProjectFile(const QString &path);
    QString suggestedSavePath() const;
    void rejectBusyRequest();

    IDocumentWorkflowUi *m_ui = nullptr;
    QStateMachine *m_machine = nullptr;
    QState *m_idleState = nullptr;
    QState *m_validatingState = nullptr;
    QState *m_awaitingSaveDecisionState = nullptr;
    QState *m_awaitingSavePathState = nullptr;
    QState *m_savingState = nullptr;
    QState *m_startingSessionState = nullptr;
    QState *m_runningSessionState = nullptr;
    QState *m_committingState = nullptr;
    QState *m_cancelingSessionState = nullptr;
    QState *m_failedState = nullptr;
    IProjectLoadSession *m_session = nullptr;
    ProgressDialog *m_progressDialog = nullptr;
    PendingRequest m_pending;
    PreparedProject m_prepared;
    ProjectOperationError m_error;
    QString m_savePath;
    QString m_projectPath;
    // An empty name and path represent an untitled project; projectName() translates its label.
    QString m_projectName;
    QString m_lastProjectFolder;
    std::optional<TerminationMode> m_terminationAfterCancellation;
    quint64 m_nextRequestId = 0;
    bool m_busy = false;
    bool m_skipSaveGuard = false;
    bool m_resumeAfterSave = false;
    ValidationResult m_validationResult = ValidationResult::Finish;
    SaveDecisionResult m_saveDecisionResult = SaveDecisionResult::Cancel;
    SavePathResult m_savePathResult = SavePathResult::Canceled;
    SaveResult m_saveResult = SaveResult::Failed;
};

#endif // DOCUMENTWORKFLOWCONTROLLER_H
