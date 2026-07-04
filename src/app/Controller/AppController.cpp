//
// Created by FlutyDeer on 2023/12/1.
//

#include "AppController.h"

#include "AppController_p.h"
#include "AudioDecodingController.h"
#include "ClipController.h"
#include "ProjectPackageResolver.h"
#include "ProjectStatusController.h"
#include "TrackController.h"
#include "ValidationController.h"
#include "Actions/AppModel/Tempo/TempoActions.h"
#include "Actions/AppModel/TimeSignature/TimeSignatureActions.h"
#include "Interface/IMainWindow.h"
#include "Interface/IPanel.h"
#include "Model/AppModel/Track.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/Audio/subsystem/MidiSystem.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/Inference/InferController.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/ProjectConverters/MidiConverter.h"
#include "Modules/Task/TaskManager.h"
#include "Tasks/DecodeAudioTask.h"
#include "Tasks/LaunchLanguageEngineTask.h"
#include "UI/Controls/Toast.h"
#include "UI/Dialogs/Base/MessageDialog.h"
#include "UI/Dialogs/Base/ProgressDialog.h"
#include "Utils/Log.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <utility>

#include "Actions/AppModel/MasterControl/MasterControlActions.h"

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
}


AppControllerPrivate::~AppControllerPrivate() {
    clearPendingOpenDialog(true);
}

AppController::AppController(QObject *parent)
    : QObject(parent), d_ptr(new AppControllerPrivate(this)) {
    Q_D(AppController);
    AppControllerPrivate::initializeModules();
}

AppController::~AppController() {
    delete d_ptr;
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(AppController)

void AppController::newProject() {
    Q_D(AppController);
    appModel->newProject();
    historyManager->reset();
    d->updateProjectPathAndName("");
    trackController->setActiveClip(appModel->tracks().first()->clips().toList().first()->id());
    appController->setActivePanel(AppGlobal::ClipEditor);
    // Reset loop settings for new project
    appStatus->loopSettings.set(LoopSettings());
}

bool AppController::openFile(const QString &filePath, QString &errorMessage) {
    Q_D(AppController);
    if (QFile(filePath).exists()) {
        const QFileInfo info(filePath);
        const auto suffix = info.suffix().toLower();
        if (suffix == "dspx")
            return d->openDspxFile(filePath, errorMessage);
        if (suffix == "mid" || suffix == "midi")
            return d->openMidiFile(filePath, errorMessage);
        Toast::show(tr("Unrecognized file format: %1").arg(suffix));
        return false;
    }
    Toast::show(tr("File does not exist: %1").arg(filePath));
    return false;
}

void AppController::requestOpenFile(const QString &filePath) {
    Q_D(AppController);
    if (!QFile(filePath).exists()) {
        Toast::show(tr("File does not exist: %1").arg(filePath));
        return;
    }

    const QFileInfo info(filePath);
    const auto suffix = info.suffix().toLower();
    if (suffix == "mid" || suffix == "midi") {
        d->openFileAndActivateFirstClip(filePath);
        return;
    }
    if (suffix != "dspx") {
        Toast::show(tr("Unrecognized file format: %1").arg(suffix));
        return;
    }

    switch (appStatus->packageModuleStatus) {
        case AppStatus::ModuleStatus::Ready:
            d->openFileAndActivateFirstClip(filePath);
            break;
        case AppStatus::ModuleStatus::Error:
            if (d->confirmOpenWithoutPackageMetadata())
                d->openFileAndActivateFirstClip(filePath);
            break;
        case AppStatus::ModuleStatus::Unknown:
        case AppStatus::ModuleStatus::Loading:
            d->waitAndOpenDspxFile(filePath);
            break;
    }
}

bool AppController::saveProject(const QString &filePath, QString &errorMessage) {
    Q_D(AppController);
    if (!appModel->saveProject(filePath, errorMessage))
        return false;

    historyManager->setSavePoint();
    d->updateProjectPathAndName(filePath);
    d->addRecentProjectFile(filePath);
    return true;
}

void AppController::importMidiFile(const QString &filePath) {
    appModel->importMidiFile(filePath);
}

void AppController::exportMidiFile(const QString &filePath) {
    appModel->exportMidiFile(filePath);
}


void AppController::onSetTempo(const double tempo) {
    const auto model = appModel;
    const auto oldTempo = model->tempo();
    const auto newTempo = tempo > 0 ? tempo : model->tempo();
    const auto actions = new TempoActions;
    actions->editTempo(oldTempo, newTempo, model);
    actions->execute();
    historyManager->record(actions);
}

void AppController::onSetTimeSignature(const int numerator, const int denominator) {
    Q_D(AppController);
    const auto model = appModel;
    const auto oldSig = model->timeSignature();
    const auto newSig = TimeSignature(numerator, denominator);
    const auto actions = new TimeSignatureActions;
    if (AppControllerPrivate::isPowerOf2(denominator)) {
        actions->editTimeSignature(oldSig, newSig, model);
    } else {
        actions->editTimeSignature(oldSig, oldSig, model);
    }
    actions->execute();
    historyManager->record(actions);
}

void AppController::editMasterControl(const TrackControl &control) {
    const auto actions = new MasterControlActions;
    actions->editMasterControl(control, appModel);
    actions->execute();
    historyManager->record(actions);
}

void AppController::setActivePanel(const AppGlobal::PanelType panelType) {
    Q_D(AppController);
    for (const auto panel : d->m_panels)
        panel->setPanelActive(panel->panelType() == panelType);
    d->m_activePanel = panelType;
    emit activePanelChanged(panelType);
}

void AppController::onUndoRedoChanged(const bool canUndo, const QString &undoActionName,
                                      const bool canRedo, const QString &redoActionName) {
    Q_D(AppController);
    Q_UNUSED(canUndo);
    Q_UNUSED(canRedo);
    Q_UNUSED(redoActionName);
    Q_UNUSED(undoActionName);
    d->m_mainWindow->updateWindowTitle();
}

void AppController::setMainWindow(IMainWindow *window) {
    Q_D(AppController);
    d->m_mainWindow = window;
}

void AppController::initializeLanguageEngine() {
    Q_D(AppController);
    std::call_once(m_languageEngineInitialized, [this, d]() {
        const auto task = new LaunchLanguageEngineTask;
        connect(task, &LaunchLanguageEngineTask::finished, this,
                [=] { d->onRunLanguageEngineTaskFinished(task); });
        taskManager->addAndStartTask(task);
        appStatus->languageModuleStatus = AppStatus::ModuleStatus::Loading;
    });
}

void AppController::quit() {
    Q_D(AppController);
    d->m_mainWindow->quit();
}

void AppController::restart() {
    qDebug() << "restart";
    Q_D(AppController);
    d->m_mainWindow->restart();
}

QString AppController::lastProjectFolder() const {
    Q_D(const AppController);
    return d->m_lastProjectFolder;
}

QString AppController::projectPath() const {
    Q_D(const AppController);
    return d->m_projectPath;
}

QString AppController::projectName() const {
    Q_D(const AppController);
    return d->m_projectName;
}

QStringList AppController::recentProjectFiles() const {
    return appOptions->general()->recentProjectFiles;
}

void AppController::clearRecentProjectFiles() {
    auto files = appOptions->general()->recentProjectFiles;
    if (files.isEmpty())
        return;
    files.clear();
    appOptions->general()->recentProjectFiles = files;
    appOptions->saveAndNotify(AppOptionsGlobal::General);
    emit recentProjectFilesChanged(files);
}

void AppController::removeRecentProjectFile(const QString &filePath) {
    auto files = appOptions->general()->recentProjectFiles;
    const auto normalizedPath = normalizedProjectPath(filePath);
    const auto oldSize = files.size();
    files.erase(std::remove_if(files.begin(), files.end(), [&](const QString &path) {
        return projectPathsEqual(normalizedProjectPath(path), normalizedPath);
    }), files.end());
    if (files.size() == oldSize)
        return;
    appOptions->general()->recentProjectFiles = files;
    appOptions->saveAndNotify(AppOptionsGlobal::General);
    emit recentProjectFilesChanged(files);
}

void AppController::setProjectName(const QString &name) {
    Q_D(AppController);
    d->m_projectName = name;
    if (d->m_mainWindow)
        d->m_mainWindow->updateWindowTitle();
}

void AppController::registerPanel(IPanel *panel) {
    Q_D(AppController);
    d->m_panels.append(panel);
}

void AppController::setTrackAndClipPanelCollapsed(const bool trackCollapsed,
                                                  const bool clipCollapsed) {
    Q_D(AppController);
    d->m_mainWindow->setTrackAndClipPanelCollapsed(trackCollapsed, clipCollapsed);
}

void AppControllerPrivate::initializeModules() {
    InferEngine::instance();
    ProjectPackageResolver::instance();
    InferController::instance();
    ProjectStatusController::instance();
    ValidationController::instance();

    connect(appOptions, &AppOptions::optionsChanged, ThemeManager::instance(),
            &ThemeManager::onAppOptionsChanged);
    connect(appModel, &AppModel::modelChanged, audioDecodingController,
            &AudioDecodingController::onModelChanged);
    connect(appModel, &AppModel::trackChanged, audioDecodingController,
            &AudioDecodingController::onTrackChanged);
}

bool AppControllerPrivate::isPowerOf2(const int num) {
    return num > 0 && (num & num - 1) == 0;
}

void AppControllerPrivate::onRunLanguageEngineTaskFinished(LaunchLanguageEngineTask *task) {
    taskManager->removeTask(task);
    const auto status =
        task->success ? AppStatus::ModuleStatus::Ready : AppStatus::ModuleStatus::Error;
    appStatus->languageModuleStatus = status;
    delete task;
}

void AppControllerPrivate::updateProjectPathAndName(const QString &path) {
    Q_Q(AppController);
    m_projectPath = path;
    q->setProjectName(m_projectPath.isEmpty() ? tr("New Project")
                                              : QFileInfo(m_projectPath).fileName());
}

void AppControllerPrivate::addRecentProjectFile(const QString &path) {
    Q_Q(AppController);
    const QFileInfo fileInfo(path);
    if (fileInfo.suffix().compare("dspx", Qt::CaseInsensitive) != 0)
        return;

    auto files = appOptions->general()->recentProjectFiles;
    const auto normalizedPath = normalizedProjectPath(path);
    files.erase(std::remove_if(files.begin(), files.end(), [&](const QString &file) {
        return projectPathsEqual(normalizedProjectPath(file), normalizedPath);
    }), files.end());
    files.prepend(normalizedPath);
    while (files.size() > maxRecentProjectFiles)
        files.removeLast();

    appOptions->general()->recentProjectFiles = files;
    appOptions->saveAndNotify(AppOptionsGlobal::General);
    emit q->recentProjectFilesChanged(files);
}

bool AppControllerPrivate::openDspxFile(const QString &path, QString &errorMessage) {
    if (!appModel->loadProject(path, errorMessage)) {
        qCritical() << errorMessage;
        return false;
    }

    historyManager->reset();
    historyManager->setSavePoint();
    updateProjectPathAndName(path);
    m_lastProjectFolder = QFileInfo(path).dir().path();
    addRecentProjectFile(path);
    return true;
}

bool AppControllerPrivate::openMidiFile(const QString &path, QString &errorMessage) {
    Q_Q(AppController);
    AppModel resultModel;
    constexpr auto midiImport = ImportMode::NewProject;
    if (MidiConverter converter; !converter.load(path, &resultModel, errorMessage, midiImport)) {
        qCritical() << errorMessage;
        return false;
    }

    appModel->loadFromAppModel(resultModel);
    historyManager->reset();
    updateProjectPathAndName("");
    q->setProjectName(QFileInfo(path).baseName());
    m_lastProjectFolder = QFileInfo(path).dir().path();
    appStatus->loopSettings.set(LoopSettings());
    return true;
}

bool AppControllerPrivate::openFileAndActivateFirstClip(const QString &filePath) {
    Q_Q(AppController);
    QString errorMessage;
    if (!q->openFile(filePath, errorMessage))
        return false;

    const auto tracks = appModel->tracks();
    if (tracks.isEmpty())
        return true;

    const auto clips = tracks.first()->clips();
    if (clips.count() == 0)
        return true;

    trackController->setActiveClip(clips.toList().first()->id());
    q->setActivePanel(AppGlobal::ClipEditor);
    return true;
}

void AppControllerPrivate::waitAndOpenDspxFile(const QString &filePath) {
    clearPendingOpenDialog();

    m_pendingOpenFilePath = filePath;
    m_pendingOpenDialog = new ProgressDialog(true, false);
    m_pendingOpenDialog->setTitle(tr("Opening Project"));
    m_pendingOpenDialog->setMessage(tr("Scanning singer packages..."));
    connect(m_pendingOpenDialog, &ProgressDialog::canceled, this,
            &AppControllerPrivate::cancelPendingOpen);
    m_pendingOpenDialog->show();

    m_pendingOpenConnection = connect(appStatus, &AppStatus::moduleStatusChanged, this,
                                      [this](const AppStatus::ModuleType module,
                                             const AppStatus::ModuleStatus status) {
                                          if (module != AppStatus::ModuleType::Package)
                                              return;
                                          handlePendingOpenPackageStatus(status);
                                      });
    handlePendingOpenPackageStatus(appStatus->packageModuleStatus);
}

void AppControllerPrivate::cancelPendingOpen() {
    m_pendingOpenFilePath.clear();
    clearPendingOpenDialog(true);
}

void AppControllerPrivate::handlePendingOpenPackageStatus(
    const AppStatus::ModuleStatus status) {
    if (m_pendingOpenFilePath.isEmpty())
        return;

    if (status == AppStatus::ModuleStatus::Ready) {
        const auto filePath = takePendingOpenFilePath();
        clearPendingOpenDialog();
        openFileAndActivateFirstClip(filePath);
    } else if (status == AppStatus::ModuleStatus::Error) {
        const auto filePath = takePendingOpenFilePath();
        clearPendingOpenDialog();
        if (confirmOpenWithoutPackageMetadata())
            openFileAndActivateFirstClip(filePath);
    }
}

QString AppControllerPrivate::takePendingOpenFilePath() {
    return std::exchange(m_pendingOpenFilePath, {});
}

void AppControllerPrivate::clearPendingOpenDialog(const bool deleteImmediately) {
    if (m_pendingOpenConnection) {
        disconnect(m_pendingOpenConnection);
        m_pendingOpenConnection = {};
    }
    if (m_pendingOpenDialog) {
        const auto dialog = m_pendingOpenDialog;
        m_pendingOpenDialog = nullptr;
        dialog->forceClose();
        if (deleteImmediately)
            delete dialog;
        else
            dialog->deleteLater();
    }
}

bool AppControllerPrivate::confirmOpenWithoutPackageMetadata() {
    MessageDialog dialog(tr("Package scan failed"),
                         tr("Singer package metadata is not available. Open the project anyway?"));
    dialog.setTitle(tr("Package scan failed"));
    dialog.addAccentButton(tr("Open Anyway"), 1);
    dialog.addButton(tr("Cancel"), 0);
    return dialog.exec() == 1;
}
