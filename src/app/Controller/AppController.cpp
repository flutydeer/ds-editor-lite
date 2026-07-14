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
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"
#include "Model/AppModel/DrawCurve.h"
#include "Model/AppModel/Params.h"
#include "Modules/Task/TaskManager.h"
#include "Tasks/DecodeAudioTask.h"
#include "Tasks/LaunchLanguageEngineTask.h"
#include "Tasks/OpenDspxProjectTask.h"
#include "UI/Controls/Toast.h"
#include "UI/Dialogs/Base/MessageDialog.h"
#include "UI/Dialogs/Base/ProgressDialog.h"
#include "Utils/Log.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>

#include <algorithm>

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

    struct ProjectModelStats {
        qsizetype trackCount = 0;
        qsizetype clipCount = 0;
        qsizetype noteCount = 0;
        qint64 curveSampleCount = 0;
    };

    ProjectModelStats projectModelStats(const AppModel &model) {
        ProjectModelStats stats;
        stats.trackCount = model.tracks().count();
        for (const auto track : model.tracks()) {
            stats.clipCount += track->clips().count();
            for (const auto clip : track->clips()) {
                if (clip->clipType() != IClip::Singing)
                    continue;
                const auto singingClip = static_cast<SingingClip *>(clip);
                stats.noteCount += singingClip->notes().count();
                const QList<Param *> params = {
                    &singingClip->params.pitch,        &singingClip->params.expressiveness,
                    &singingClip->params.energy,       &singingClip->params.breathiness,
                    &singingClip->params.voicing,      &singingClip->params.tension,
                    &singingClip->params.mouthOpening, &singingClip->params.gender,
                    &singingClip->params.velocity,     &singingClip->params.toneShift,
                };
                for (const auto param : params) {
                    for (const auto type : {Param::Original, Param::Edited, Param::Envelope}) {
                        for (const auto curve : param->curves(type)) {
                            if (curve->type() == Curve::Draw)
                                stats.curveSampleCount +=
                                    static_cast<DrawCurve *>(curve)->values().count();
                        }
                    }
                }
            }
        }
        return stats;
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

    d->requestOpenDspxFile(filePath);
}

bool AppController::saveProject(const QString &filePath, QString &errorMessage) {
    Q_D(AppController);
    DspxProjectConverter converter;
    if (!converter.save(filePath, appModel, errorMessage))
        return false;

    historyManager->setSavePoint();
    d->updateProjectPathAndName(filePath);
    d->addRecentProjectFile(filePath);
    return true;
}

bool AppController::importMidiFile(const QString &filePath) {
    Q_D(AppController);
    QString errMsg;
    int midiImport = MidiConverter::midiImportHandler();

    if (midiImport == -1) {
        errMsg = "User canceled the import.";
        return false;
    }

    AppModel resultModel;
    MidiConverter converter;
    const auto ok = converter.load(filePath, &resultModel, errMsg,
                                   static_cast<IProjectConverter::ImportMode>(midiImport));
    Log::i("Midi importer", errMsg);
    if (ok) {
        SingingClipPhonemeNormalizer::normalizeEditedOffsets(resultModel);
        if (midiImport == IProjectConverter::ImportMode::NewProject) {
            appModel->loadFromAppModel(resultModel);
        } else if (midiImport == IProjectConverter::ImportMode::AppendToProject) {
            appModel->setTimeSignature(resultModel.timeSignature());
            appModel->setTempo(resultModel.tempo());
            for (const auto track : resultModel.tracks()) {
                appModel->appendTrack(track);
            }
        }
    }
    return ok;
}

bool AppController::exportMidiFile(const QString &filePath) {
    MidiConverter converter;
    QString errMsg;
    Log::i("Midi exporter", errMsg);
    return converter.save(filePath, appModel, errMsg);
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
    emit q->recentProjectFilesChanged(files);
}

bool AppControllerPrivate::openDspxFile(const QString &path, QString &errorMessage) {
    DspxProjectConverter converter;
    AppModel resultModel;
    const auto ok =
        converter.load(path, &resultModel, errorMessage, IProjectConverter::ImportMode::NewProject);
    if (!ok) {
        qCritical() << errorMessage;
        return false;
    }

    SingingClipPhonemeNormalizer::normalizeEditedOffsets(resultModel);
    appModel->loadFromAppModel(resultModel);
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

    activateFirstClip();
    return true;
}

void AppControllerPrivate::requestOpenDspxFile(const QString &filePath) {
    cancelPendingOpen();
    ++m_openRequestId;
    m_pendingOpenFilePath = filePath;
    m_projectOpenTimer.start();
    createPendingOpenDialog();

    switch (appStatus->packageModuleStatus) {
        case AppStatus::ModuleStatus::Ready:
            startOpenDspxTask();
            break;
        case AppStatus::ModuleStatus::Error:
            clearPendingOpenDialog();
            if (confirmOpenWithoutPackageMetadata()) {
                createPendingOpenDialog();
                startOpenDspxTask();
            } else {
                cancelPendingOpen();
            }
            break;
        case AppStatus::ModuleStatus::Unknown:
        case AppStatus::ModuleStatus::Loading:
            waitAndOpenDspxFile();
            break;
    }
}

void AppControllerPrivate::createPendingOpenDialog() {
    if (m_pendingOpenDialog)
        return;
    m_pendingOpenDialog =
        new ProgressDialog(true, false, dynamic_cast<QWidget *>(m_mainWindow));
    m_pendingOpenDialog->setTitle(tr("Opening Project"));
    connect(m_pendingOpenDialog, &ProgressDialog::canceled, this,
            &AppControllerPrivate::cancelPendingOpen);
    m_pendingOpenDialog->show();
}

void AppControllerPrivate::waitAndOpenDspxFile() {
    m_projectOpenState = ProjectOpenState::WaitingPackages;
    m_pendingOpenDialog->setMessage(tr("Scanning singer packages..."));

    m_pendingOpenConnection =
        connect(appStatus, &AppStatus::moduleStatusChanged, this,
                [this](const AppStatus::ModuleType module, const AppStatus::ModuleStatus status) {
                    if (module != AppStatus::ModuleType::Package)
                        return;
                    handlePendingOpenPackageStatus(status);
                });
    handlePendingOpenPackageStatus(appStatus->packageModuleStatus);
}

void AppControllerPrivate::startOpenDspxTask() {
    if (m_pendingOpenFilePath.isEmpty() || m_openProjectTask)
        return;

    if (m_pendingOpenConnection) {
        disconnect(m_pendingOpenConnection);
        m_pendingOpenConnection = {};
    }

    m_projectOpenState = ProjectOpenState::Loading;
    createPendingOpenDialog();
    const auto task = new OpenDspxProjectTask(m_pendingOpenFilePath, m_openRequestId);
    m_openProjectTask = task;
    updateOpenProjectDialog(task->status());
    connect(task, &Task::statusUpdated, this, [this, task](const TaskStatus &status) {
        if (task == m_openProjectTask && task->requestId() == m_openRequestId)
            updateOpenProjectDialog(status);
    });
    connect(task, &Task::finished, this, [this, task] { handleOpenDspxTaskFinished(task); });
    taskManager->addAndStartTask(task);
}

void AppControllerPrivate::handleOpenDspxTaskFinished(OpenDspxProjectTask *task) {
    const bool isCurrent = task == m_openProjectTask && task->requestId() == m_openRequestId;
    if (taskManager->tasks().contains(task))
        taskManager->removeTask(task);

    if (!isCurrent || task->terminated()) {
        delete task;
        return;
    }

    m_openProjectTask = nullptr;
    auto parseResult = task->takeResult();
    if (!parseResult.success()) {
        const auto errorMessage = parseResult.errorMessage;
        m_pendingOpenFilePath.clear();
        m_projectOpenState = ProjectOpenState::Idle;
        clearPendingOpenDialog();
        showOpenProjectError(errorMessage);
        delete task;
        return;
    }

    m_projectOpenState = ProjectOpenState::Committing;
    m_pendingOpenDialog->setCancellationEnabled(false);
    m_pendingOpenDialog->setMessage(tr("Applying project..."));
    m_pendingOpenDialog->setProgressIndeterminate(true);

    QElapsedTimer stageTimer;
    stageTimer.start();
    AppModel resultModel;
    LoopSettings loopSettings;
    QString errorMessage;
    DspxProjectConverter converter;
    if (!converter.loadParsedProject(*parseResult.model, &resultModel, loopSettings, errorMessage,
                                     IProjectConverter::ImportMode::NewProject)) {
        m_pendingOpenFilePath.clear();
        m_projectOpenState = ProjectOpenState::Idle;
        clearPendingOpenDialog();
        showOpenProjectError(errorMessage);
        delete task;
        return;
    }
    const auto materializeMs = stageTimer.elapsed();

    stageTimer.restart();
    SingingClipPhonemeNormalizer::normalizeEditedOffsets(resultModel);
    const auto normalizeMs = stageTimer.elapsed();
    const auto stats = projectModelStats(resultModel);

    appStatus->loopSettings.set(loopSettings);
    stageTimer.restart();
    appModel->loadFromAppModel(resultModel);
    const auto modelChangedMs = stageTimer.elapsed();

    historyManager->reset();
    historyManager->setSavePoint();
    updateProjectPathAndName(m_pendingOpenFilePath);
    m_lastProjectFolder = QFileInfo(m_pendingOpenFilePath).dir().path();
    addRecentProjectFile(m_pendingOpenFilePath);

    stageTimer.restart();
    activateFirstClip();
    const auto activateClipMs = stageTimer.elapsed();

    qInfo().noquote() << QString(
                             "Project open timings: file=%1 bytes, read=%2 ms, parse=%3 ms, "
                             "materialize=%4 ms, normalize=%5 ms, modelChanged=%6 ms, "
                             "activateClip=%7 ms, total=%8 ms, tracks=%9, clips=%10, notes=%11, "
                             "curveSamples=%12")
                             .arg(task->fileSize())
                             .arg(task->readElapsedMs())
                             .arg(task->parseElapsedMs())
                             .arg(materializeMs)
                             .arg(normalizeMs)
                             .arg(modelChangedMs)
                             .arg(activateClipMs)
                             .arg(m_projectOpenTimer.elapsed())
                             .arg(stats.trackCount)
                             .arg(stats.clipCount)
                             .arg(stats.noteCount)
                             .arg(stats.curveSampleCount);

    m_pendingOpenFilePath.clear();
    m_projectOpenState = ProjectOpenState::Idle;
    clearPendingOpenDialog();
    delete task;
}

void AppControllerPrivate::updateOpenProjectDialog(const TaskStatus &status) const {
    if (!m_pendingOpenDialog)
        return;
    m_pendingOpenDialog->setTitle(status.title);
    m_pendingOpenDialog->setMessage(status.message);
    m_pendingOpenDialog->setProgressRange(status.minimum, status.maximum);
    m_pendingOpenDialog->setProgressValue(status.progress);
    m_pendingOpenDialog->setProgressIndeterminate(status.isIndetermine);
    m_pendingOpenDialog->setProgressStatus(status.runningStatus);
}

void AppControllerPrivate::showOpenProjectError(const QString &errorMessage) const {
    MessageDialog dialog;
    dialog.setWindowTitle(tr("Error"));
    dialog.setTitle(tr("Failed to open project"));
    dialog.setMessage(errorMessage);
    dialog.addAccentButton(tr("OK"), 1);
    dialog.exec();
}

void AppControllerPrivate::activateFirstClip() {
    Q_Q(AppController);
    const auto tracks = appModel->tracks();
    if (tracks.isEmpty())
        return;
    const auto clips = tracks.first()->clips();
    if (clips.count() == 0)
        return;
    trackController->setActiveClip(clips.toList().first()->id());
    q->setActivePanel(AppGlobal::ClipEditor);
}

void AppControllerPrivate::cancelPendingOpen() {
    ++m_openRequestId;
    m_pendingOpenFilePath.clear();
    m_projectOpenState = ProjectOpenState::Idle;
    if (m_openProjectTask) {
        const auto task = m_openProjectTask;
        m_openProjectTask = nullptr;
        taskManager->terminateTask(task);
        if (taskManager->tasks().contains(task))
            taskManager->removeTask(task);
    }
    clearPendingOpenDialog();
}

void AppControllerPrivate::handlePendingOpenPackageStatus(const AppStatus::ModuleStatus status) {
    if (m_pendingOpenFilePath.isEmpty())
        return;

    if (status == AppStatus::ModuleStatus::Ready) {
        startOpenDspxTask();
    } else if (status == AppStatus::ModuleStatus::Error) {
        clearPendingOpenDialog();
        if (confirmOpenWithoutPackageMetadata()) {
            createPendingOpenDialog();
            startOpenDspxTask();
        } else {
            cancelPendingOpen();
        }
    }
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
