#include "DocumentWorkflowController.h"

#include "DspxLoadSession.h"
#include "IDocumentWorkflowUi.h"
#include "IProjectLoadSession.h"
#include "LegacyMidiLoadSession.h"
#include "Controller/Actions/AppModel/ImportProjectActions.h"
#include "Controller/AppController.h"
#include "Controller/TrackController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/ProjectConverters/DspxProjectConverter.h"
#include "UI/Dialogs/Base/ProgressDialog.h"
#include "Utils/ConditionalTransition.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QState>
#include <QStateMachine>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace {
    constexpr int maxRecentProjectFiles = 10;

    QString normalizedProjectPath(const QString &path) {
        return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    }

    bool projectPathsEqual(const QString &lhs, const QString &rhs) {
#ifdef Q_OS_WIN
        return QString::compare(lhs, rhs, Qt::CaseInsensitive) == 0;
#else
        return lhs == rhs;
#endif
    }

    void addGuardedTransition(QState *source, QObject *sender, const char *signal,
                              QAbstractState *target, std::function<bool()> guard) {
        const auto transition = new ConditionalTransition(sender, signal, std::move(guard));
        transition->setTargetState(target);
        source->addTransition(transition);
    }
}

DocumentWorkflowController::DocumentWorkflowController(QObject *parent)
    : QObject(parent),
      m_lastProjectFolder(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)) {
    initializeStateMachine();
}

DocumentWorkflowController::~DocumentWorkflowController() {
    closeProgressDialog();
    cleanSession();
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(DocumentWorkflowController)

void DocumentWorkflowController::setUi(IDocumentWorkflowUi *ui) {
    m_ui = ui;
}

void DocumentWorkflowController::initializeNewDocument() {
    AppModel newModel;
    newModel.newProject();
    historyManager->reset(HistoryManager::ResetState::Saved);
    appModel->replaceProject(newModel.takeProjectData());
    appStatus->loopSettings.set(LoopSettings());
    updateProjectIdentity({});
    historyManager->reset(HistoryManager::ResetState::Saved);
    activateFirstClip();
}

void DocumentWorkflowController::requestNew() {
    begin(DocumentOperation::New);
}

void DocumentWorkflowController::requestOpen(const QString &path) {
    begin(DocumentOperation::Open, path);
}

void DocumentWorkflowController::requestImport(const QString &path) {
    begin(DocumentOperation::Import, path);
}

void DocumentWorkflowController::requestSave() {
    begin(DocumentOperation::Save);
}

void DocumentWorkflowController::requestSaveAs() {
    begin(DocumentOperation::SaveAs);
}

void DocumentWorkflowController::requestTermination(const TerminationMode mode) {
    if (m_busy) {
        if (m_session) {
            m_terminationAfterCancellation = mode;
            closeProgressDialog();
            emit cancelSessionRequested();
        } else {
            rejectBusyRequest();
        }
        return;
    }

    m_pending = {};
    m_pending.requestId = ++m_nextRequestId;
    m_pending.termination = mode;
    m_busy = true;
    emit busyChanged(true);
    emit beginRequested();
}

void DocumentWorkflowController::cancelCurrentOperation() {
    if (!m_busy || !m_session)
        return;
    closeProgressDialog();
    emit cancelSessionRequested();
}

bool DocumentWorkflowController::busy() const {
    return m_busy;
}

QString DocumentWorkflowController::projectPath() const {
    return m_projectPath;
}

QString DocumentWorkflowController::projectName() const {
    return m_projectPath.isEmpty() && m_projectName.isEmpty() ? tr("New Project") : m_projectName;
}

QString DocumentWorkflowController::lastProjectFolder() const {
    return m_lastProjectFolder;
}

QStringList DocumentWorkflowController::recentProjectFiles() const {
    return appOptions->general()->recentProjectFiles;
}

void DocumentWorkflowController::clearRecentProjectFiles() {
    if (appOptions->general()->recentProjectFiles.isEmpty())
        return;
    appOptions->general()->recentProjectFiles.clear();
    appOptions->saveAndNotify(AppOptionsGlobal::General);
    emit recentProjectFilesChanged({});
}

void DocumentWorkflowController::removeRecentProjectFile(const QString &filePath) {
    auto files = appOptions->general()->recentProjectFiles;
    const auto normalizedPath = normalizedProjectPath(filePath);
    const auto oldSize = files.size();
    files.erase(std::remove_if(files.begin(), files.end(),
                               [&](const QString &path) {
                                   return projectPathsEqual(normalizedProjectPath(path),
                                                            normalizedPath);
                               }),
                files.end());
    if (files.size() == oldSize)
        return;
    appOptions->general()->recentProjectFiles = files;
    appOptions->saveAndNotify(AppOptionsGlobal::General);
    emit recentProjectFilesChanged(files);
}

void DocumentWorkflowController::initializeStateMachine() {
    m_machine = new QStateMachine(this);
    m_idleState = new QState(m_machine);
    m_validatingState = new QState(m_machine);
    m_awaitingSaveDecisionState = new QState(m_machine);
    m_awaitingSavePathState = new QState(m_machine);
    m_savingState = new QState(m_machine);
    m_startingSessionState = new QState(m_machine);
    m_runningSessionState = new QState(m_machine);
    m_committingState = new QState(m_machine);
    m_cancelingSessionState = new QState(m_machine);
    m_failedState = new QState(m_machine);

    m_machine->setInitialState(m_idleState);
    m_idleState->addTransition(this, &DocumentWorkflowController::beginRequested,
                               m_validatingState);

    addGuardedTransition(
        m_validatingState, this, SIGNAL(validationCompleted()), m_awaitingSaveDecisionState,
        [this] { return m_validationResult == ValidationResult::AwaitSaveDecision; });
    addGuardedTransition(m_validatingState, this, SIGNAL(validationCompleted()),
                         m_awaitingSavePathState,
                         [this] { return m_validationResult == ValidationResult::AwaitSavePath; });
    addGuardedTransition(m_validatingState, this, SIGNAL(validationCompleted()), m_savingState,
                         [this] { return m_validationResult == ValidationResult::Save; });
    addGuardedTransition(m_validatingState, this, SIGNAL(validationCompleted()),
                         m_startingSessionState,
                         [this] { return m_validationResult == ValidationResult::StartSession; });
    addGuardedTransition(m_validatingState, this, SIGNAL(validationCompleted()), m_committingState,
                         [this] { return m_validationResult == ValidationResult::Commit; });
    addGuardedTransition(m_validatingState, this, SIGNAL(validationCompleted()), m_idleState,
                         [this] { return m_validationResult == ValidationResult::Finish; });
    addGuardedTransition(m_validatingState, this, SIGNAL(validationCompleted()), m_failedState,
                         [this] { return m_validationResult == ValidationResult::Fail; });

    addGuardedTransition(
        m_awaitingSaveDecisionState, this, SIGNAL(saveDecisionCompleted()), m_savingState,
        [this] { return m_saveDecisionResult == SaveDecisionResult::SaveWithPath; });
    addGuardedTransition(
        m_awaitingSaveDecisionState, this, SIGNAL(saveDecisionCompleted()), m_awaitingSavePathState,
        [this] { return m_saveDecisionResult == SaveDecisionResult::SaveWithoutPath; });
    addGuardedTransition(m_awaitingSaveDecisionState, this, SIGNAL(saveDecisionCompleted()),
                         m_validatingState,
                         [this] { return m_saveDecisionResult == SaveDecisionResult::Discard; });
    addGuardedTransition(m_awaitingSaveDecisionState, this, SIGNAL(saveDecisionCompleted()),
                         m_idleState,
                         [this] { return m_saveDecisionResult == SaveDecisionResult::Cancel; });

    addGuardedTransition(m_awaitingSavePathState, this, SIGNAL(savePathSelectionCompleted()),
                         m_savingState,
                         [this] { return m_savePathResult == SavePathResult::Selected; });
    addGuardedTransition(m_awaitingSavePathState, this, SIGNAL(savePathSelectionCompleted()),
                         m_idleState,
                         [this] { return m_savePathResult == SavePathResult::Canceled; });

    addGuardedTransition(m_savingState, this, SIGNAL(saveCompleted()), m_validatingState,
                         [this] { return m_saveResult == SaveResult::SucceededAndResume; });
    addGuardedTransition(m_savingState, this, SIGNAL(saveCompleted()), m_idleState,
                         [this] { return m_saveResult == SaveResult::SucceededAndFinish; });
    addGuardedTransition(m_savingState, this, SIGNAL(saveCompleted()), m_awaitingSaveDecisionState,
                         [this] { return m_saveResult == SaveResult::FailedAndResume; });
    addGuardedTransition(m_savingState, this, SIGNAL(saveCompleted()), m_failedState,
                         [this] { return m_saveResult == SaveResult::Failed; });

    m_startingSessionState->addTransition(this, &DocumentWorkflowController::sessionStarted,
                                          m_runningSessionState);
    m_startingSessionState->addTransition(this, &DocumentWorkflowController::operationFailed,
                                          m_failedState);
    m_runningSessionState->addTransition(this, &DocumentWorkflowController::sessionReadyEvent,
                                         m_committingState);
    m_runningSessionState->addTransition(this, &DocumentWorkflowController::sessionFailedEvent,
                                         m_failedState);
    m_runningSessionState->addTransition(this, &DocumentWorkflowController::sessionCanceledEvent,
                                         m_idleState);
    m_runningSessionState->addTransition(this, &DocumentWorkflowController::cancelSessionRequested,
                                         m_cancelingSessionState);
    addGuardedTransition(m_cancelingSessionState, this, SIGNAL(sessionCanceledEvent()),
                         m_validatingState, [this] { return m_pending.termination.has_value(); });
    addGuardedTransition(m_cancelingSessionState, this, SIGNAL(sessionCanceledEvent()), m_idleState,
                         [this] { return !m_pending.termination.has_value(); });
    m_cancelingSessionState->addTransition(this, &DocumentWorkflowController::sessionFailedEvent,
                                           m_failedState);

    m_committingState->addTransition(this, &DocumentWorkflowController::commitFinished,
                                     m_idleState);
    m_committingState->addTransition(this, &DocumentWorkflowController::operationFailed,
                                     m_failedState);
    m_failedState->addTransition(this, &DocumentWorkflowController::failureHandled, m_idleState);

    connect(m_idleState, &QState::entered, this, &DocumentWorkflowController::enterIdle);
    connect(m_validatingState, &QState::entered, this,
            &DocumentWorkflowController::validatePendingRequest);
    connect(m_awaitingSaveDecisionState, &QState::entered, this,
            &DocumentWorkflowController::askSaveDecision);
    connect(m_awaitingSavePathState, &QState::entered, this,
            &DocumentWorkflowController::askSavePath);
    connect(m_savingState, &QState::entered, this, &DocumentWorkflowController::performSave);
    connect(m_startingSessionState, &QState::entered, this,
            &DocumentWorkflowController::createSession);
    connect(m_runningSessionState, &QState::entered, this,
            &DocumentWorkflowController::startSession);
    connect(m_committingState, &QState::entered, this,
            &DocumentWorkflowController::commitPreparedProject);
    connect(m_cancelingSessionState, &QState::entered, this, [this] {
        if (m_session)
            m_session->cancel();
        else
            emit sessionCanceledEvent();
    });
    connect(m_failedState, &QState::entered, this, &DocumentWorkflowController::enterFailed);
    m_machine->start();
}

void DocumentWorkflowController::begin(const DocumentOperation operation, const QString &filePath) {
    if (m_busy) {
        rejectBusyRequest();
        return;
    }
    m_pending = {};
    m_pending.requestId = ++m_nextRequestId;
    m_pending.operation = operation;
    m_pending.filePath = filePath;
    m_busy = true;
    emit busyChanged(true);
    emit beginRequested();
}

void DocumentWorkflowController::validatePendingRequest() {
    if (m_pending.operation == DocumentOperation::Open ||
        m_pending.operation == DocumentOperation::Import) {
        if (!QFile::exists(m_pending.filePath)) {
            m_error = {tr("File not found"), tr("File does not exist: %1").arg(m_pending.filePath)};
            m_validationResult = ValidationResult::Fail;
            emit validationCompleted();
            return;
        }
        const auto suffix = QFileInfo(m_pending.filePath).suffix().toLower();
        const bool supportedOpen = suffix == "dspx" || suffix == "mid" || suffix == "midi";
        const bool supportedImport = suffix == "mid" || suffix == "midi";
        if ((m_pending.operation == DocumentOperation::Open && !supportedOpen) ||
            (m_pending.operation == DocumentOperation::Import && !supportedImport)) {
            m_error = {tr("Unsupported file"), tr("Unrecognized file format: %1").arg(suffix)};
            m_validationResult = ValidationResult::Fail;
            emit validationCompleted();
            return;
        }
    }

    const bool needsGuard = m_pending.termination.has_value() ||
                            m_pending.operation == DocumentOperation::New ||
                            m_pending.operation == DocumentOperation::Open;
    if (needsGuard && !m_skipSaveGuard && !historyManager->isOnSavePoint()) {
        m_resumeAfterSave = true;
        m_validationResult = ValidationResult::AwaitSaveDecision;
        emit validationCompleted();
        return;
    }

    m_skipSaveGuard = false;
    if (m_pending.termination) {
        emit terminationApproved(*m_pending.termination);
        m_validationResult = ValidationResult::Finish;
    } else if (m_pending.operation == DocumentOperation::Save) {
        m_resumeAfterSave = false;
        if (m_projectPath.isEmpty())
            m_validationResult = ValidationResult::AwaitSavePath;
        else {
            m_savePath = m_projectPath;
            m_validationResult = ValidationResult::Save;
        }
    } else if (m_pending.operation == DocumentOperation::SaveAs) {
        m_resumeAfterSave = false;
        m_validationResult = ValidationResult::AwaitSavePath;
    } else if (m_pending.operation == DocumentOperation::New) {
        prepareNewProject();
        m_validationResult = ValidationResult::Commit;
    } else {
        m_validationResult = ValidationResult::StartSession;
    }
    emit validationCompleted();
}

void DocumentWorkflowController::askSaveDecision() {
    if (!m_ui) {
        m_saveDecisionResult = SaveDecisionResult::Cancel;
        emit saveDecisionCompleted();
        return;
    }
    switch (m_ui->askDocumentSaveDecision()) {
        case SaveDecision::Save:
            if (m_projectPath.isEmpty())
                m_saveDecisionResult = SaveDecisionResult::SaveWithoutPath;
            else {
                m_savePath = m_projectPath;
                m_saveDecisionResult = SaveDecisionResult::SaveWithPath;
            }
            break;
        case SaveDecision::Discard:
            m_skipSaveGuard = true;
            m_saveDecisionResult = SaveDecisionResult::Discard;
            break;
        case SaveDecision::Cancel:
            m_saveDecisionResult = SaveDecisionResult::Cancel;
            break;
    }
    emit saveDecisionCompleted();
}

void DocumentWorkflowController::askSavePath() {
    if (!m_ui) {
        m_savePathResult = SavePathResult::Canceled;
        emit savePathSelectionCompleted();
        return;
    }
    m_savePath = m_ui->chooseDocumentSavePath(suggestedSavePath());
    m_savePathResult = m_savePath.isEmpty() ? SavePathResult::Canceled : SavePathResult::Selected;
    emit savePathSelectionCompleted();
}

void DocumentWorkflowController::performSave() {
    QString errorMessage;
    DspxProjectConverter converter;
    if (converter.save(m_savePath, appModel, errorMessage)) {
        historyManager->setSavePoint();
        updateProjectIdentity(m_savePath);
        m_lastProjectFolder = QFileInfo(m_savePath).dir().path();
        addRecentProjectFile(m_savePath);
        if (m_resumeAfterSave) {
            m_skipSaveGuard = true;
            m_saveResult = SaveResult::SucceededAndResume;
        } else {
            m_saveResult = SaveResult::SucceededAndFinish;
        }
    } else {
        m_error = {tr("Failed to save project"), errorMessage};
        if (m_resumeAfterSave && m_ui)
            m_ui->showDocumentWorkflowError(m_error);
        if (m_resumeAfterSave)
            m_saveResult = SaveResult::FailedAndResume;
        else
            m_saveResult = SaveResult::Failed;
    }
    emit saveCompleted();
}

void DocumentWorkflowController::createSession() {
    const auto suffix = QFileInfo(m_pending.filePath).suffix().toLower();
    const auto purpose = m_pending.operation == DocumentOperation::Import
                             ? ProjectLoadPurpose::Import
                             : ProjectLoadPurpose::Open;
    if (suffix == "dspx" && purpose == ProjectLoadPurpose::Open) {
        m_session = new DspxLoadSession(m_pending.filePath, m_pending.requestId, m_ui, this);
    } else if (suffix == "mid" || suffix == "midi") {
        m_session =
            new LegacyMidiLoadSession(m_pending.filePath, purpose, m_pending.requestId, this);
    } else {
        m_error = {tr("Unsupported file"), tr("This operation is not supported.")};
        emit operationFailed();
        return;
    }

    const auto session = m_session;
    connect(session, &IProjectLoadSession::progressChanged, this,
            [this, session](const ProjectLoadProgress &progress) {
                if (session == m_session && session->requestId() == m_pending.requestId)
                    ensureProgressDialog(progress);
            });
    connect(session, &IProjectLoadSession::ready, this,
            [this, session] { handleSessionReady(session); });
    connect(session, &IProjectLoadSession::failed, this,
            [this, session](const ProjectOperationError &error) {
                handleSessionFailed(session, error);
            });
    connect(session, &IProjectLoadSession::canceled, this,
            [this, session] { handleSessionCanceled(session); });
    emit sessionStarted();
}

void DocumentWorkflowController::startSession() {
    const auto session = m_session;
    QTimer::singleShot(0, this, [this, session] {
        if (session == m_session)
            session->start();
    });
}

void DocumentWorkflowController::commitPreparedProject() {
    if (m_progressDialog) {
        m_progressDialog->setCancellationEnabled(false);
        m_progressDialog->setMessage(tr("Applying project..."));
        m_progressDialog->setProgressIndeterminate(true);
    }

    if (auto payload = std::get_if<ReplaceProjectPayload>(&m_prepared)) {
        commitReplace(std::move(*payload));
    } else if (auto payload = std::get_if<AppendProjectPayload>(&m_prepared)) {
        commitAppend(std::move(*payload));
    } else {
        m_error = {tr("Failed to apply project"), tr("The prepared project is empty.")};
        emit operationFailed();
        return;
    }
    emit commitFinished();
}

void DocumentWorkflowController::enterIdle() {
    closeProgressDialog();
    cleanSession();
    m_pending = {};
    m_prepared = {};
    m_error = {};
    m_savePath.clear();
    m_terminationAfterCancellation.reset();
    m_skipSaveGuard = false;
    m_resumeAfterSave = false;
    if (m_busy) {
        m_busy = false;
        emit busyChanged(false);
    }
}

void DocumentWorkflowController::enterFailed() {
    closeProgressDialog();
    cleanSession();
    if (m_ui && !m_error.message.isEmpty())
        m_ui->showDocumentWorkflowError(m_error);
    QTimer::singleShot(0, this, [this] { emit failureHandled(); });
}

void DocumentWorkflowController::cleanSession() {
    if (!m_session)
        return;
    const auto session = m_session;
    m_session = nullptr;
    session->deleteLater();
}

void DocumentWorkflowController::ensureProgressDialog(const ProjectLoadProgress &progress) {
    if (!m_progressDialog) {
        m_progressDialog =
            new ProgressDialog(true, false, m_ui ? m_ui->documentWorkflowParentWidget() : nullptr);
        connect(m_progressDialog, &ProgressDialog::canceled, this,
                &DocumentWorkflowController::cancelCurrentOperation);
        m_progressDialog->show();
    }
    m_progressDialog->setTitle(progress.title);
    m_progressDialog->setMessage(progress.message);
    m_progressDialog->setProgressRange(progress.minimum, progress.maximum);
    m_progressDialog->setProgressValue(progress.value);
    m_progressDialog->setProgressIndeterminate(progress.indeterminate);
}

void DocumentWorkflowController::closeProgressDialog() {
    if (!m_progressDialog)
        return;
    const auto dialog = m_progressDialog;
    m_progressDialog = nullptr;
    dialog->forceClose();
    dialog->deleteLater();
}

void DocumentWorkflowController::handleSessionReady(IProjectLoadSession *session) {
    if (session != m_session || session->requestId() != m_pending.requestId)
        return;
    m_prepared = session->takeResult();
    emit sessionReadyEvent();
}

void DocumentWorkflowController::handleSessionFailed(IProjectLoadSession *session,
                                                     const ProjectOperationError &error) {
    if (session != m_session || session->requestId() != m_pending.requestId)
        return;
    m_error = error;
    emit sessionFailedEvent();
}

void DocumentWorkflowController::handleSessionCanceled(IProjectLoadSession *session) {
    if (session != m_session || session->requestId() != m_pending.requestId)
        return;
    if (m_terminationAfterCancellation) {
        m_pending = {};
        m_pending.requestId = ++m_nextRequestId;
        m_pending.termination = *m_terminationAfterCancellation;
        m_terminationAfterCancellation.reset();
        m_skipSaveGuard = false;
    }
    emit sessionCanceledEvent();
}

void DocumentWorkflowController::prepareNewProject() {
    AppModel newModel;
    newModel.newProject();
    ReplaceProjectPayload payload;
    payload.model = newModel.takeProjectData();
    payload.loopSettings = LoopSettings();
    payload.sourceKind = ProjectSourceKind::Foreign;
    m_prepared = std::move(payload);
}

void DocumentWorkflowController::commitReplace(ReplaceProjectPayload &&payload) {
    const bool isNew = m_pending.operation == DocumentOperation::New;
    const bool saved = isNew || payload.sourceKind == ProjectSourceKind::Native;
    historyManager->reset(HistoryManager::ResetState::Saved);
    appModel->replaceProject(std::move(payload.model));
    appStatus->loopSettings.set(payload.loopSettings);

    if (payload.sourceKind == ProjectSourceKind::Native) {
        updateProjectIdentity(payload.sourcePath);
        addRecentProjectFile(payload.sourcePath);
    } else {
        updateProjectIdentity({}, payload.displayName);
    }
    if (!payload.sourcePath.isEmpty())
        m_lastProjectFolder = QFileInfo(payload.sourcePath).dir().path();
    historyManager->reset(saved ? HistoryManager::ResetState::Saved
                                : HistoryManager::ResetState::Unsaved);
    activateFirstClip();
}

void DocumentWorkflowController::commitAppend(AppendProjectPayload &&payload) {
    QList<Track *> importedTracks;
    importedTracks.reserve(static_cast<qsizetype>(payload.model.tracks.size()));
    for (const auto &track : payload.model.tracks)
        importedTracks.append(track.get());

    const auto actions = new ImportProjectActions(std::move(payload.model), payload.importTempo,
                                                  payload.importTimeSignature, appModel);
    actions->execute();
    historyManager->record(actions);
    if (!payload.sourcePath.isEmpty())
        m_lastProjectFolder = QFileInfo(payload.sourcePath).dir().path();
    activateFirstClip(importedTracks);
}

void DocumentWorkflowController::activateFirstClip(const QList<Track *> &preferredTracks) {
    const auto &tracks = preferredTracks.isEmpty() ? appModel->tracks() : preferredTracks;
    for (const auto track : tracks) {
        const auto clips = track->clips();
        if (clips.count() == 0)
            continue;
        trackController->setActiveClip(clips.toList().first()->id());
        appController->setActivePanel(AppGlobal::ClipEditor);
        return;
    }
}

void DocumentWorkflowController::updateProjectIdentity(const QString &path, const QString &name) {
    m_projectPath = path;
    m_projectName = !name.isEmpty()            ? name
                    : !m_projectPath.isEmpty() ? QFileInfo(m_projectPath).fileName()
                                               : QString();
    emit documentIdentityChanged();
}

void DocumentWorkflowController::addRecentProjectFile(const QString &path) {
    if (QFileInfo(path).suffix().compare("dspx", Qt::CaseInsensitive) != 0)
        return;
    auto files = appOptions->general()->recentProjectFiles;
    const auto normalizedPath = normalizedProjectPath(path);
    files.erase(std::remove_if(files.begin(), files.end(),
                               [&](const QString &file) {
                                   return projectPathsEqual(normalizedProjectPath(file),
                                                            normalizedPath);
                               }),
                files.end());
    files.prepend(normalizedPath);
    while (files.size() > maxRecentProjectFiles)
        files.removeLast();
    appOptions->general()->recentProjectFiles = files;
    appOptions->saveAndNotify(AppOptionsGlobal::General);
    emit recentProjectFilesChanged(files);
}

QString DocumentWorkflowController::suggestedSavePath() const {
    return m_projectPath.isEmpty() ? m_lastProjectFolder + "/" + projectName() : m_projectPath;
}

void DocumentWorkflowController::rejectBusyRequest() {
    if (m_ui)
        m_ui->showDocumentWorkflowBusy();
}
