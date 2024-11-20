//
// Created by FlutyDeer on 2023/12/1.
//

#include "AppController.h"

#include "AppController_p.h"
#include "AudioDecodingController.h"
#include "ClipController.h"
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
#include "Utils/Log.h"

#include <QDir>
#include <QFileInfo>

AppController::AppController() : d_ptr(new AppControllerPrivate(this)) {
    Q_D(AppController);
    AppControllerPrivate::initializeModules();

    auto task = new LaunchLanguageEngineTask;
    connect(task, &LaunchLanguageEngineTask::finished, this,
            [=] { d->onRunLanguageEngineTaskFinished(task); });
    taskManager->addAndStartTask(task);
    appStatus->languageModuleStatus = AppStatus::ModuleStatus::Loading;
}

AppController::~AppController() {
    delete d_ptr;
}

void AppController::newProject() {
    Q_D(AppController);
    appModel->newProject();
    historyManager->reset();
    d->updateProjectPathAndName("");
    trackController->setActiveClip(appModel->tracks().first()->clips().toList().first()->id());
    appController->setActivePanel(AppGlobal::ClipEditor);
}

bool AppController::openFile(const QString &filePath, QString &errorMessage) {
    Q_D(AppController);
    if (QFile(filePath).exists()) {
        const QFileInfo info(filePath);
        const auto suffix = info.suffix().toLower();
        if (suffix == "dspx")
            return d->openDspxFile(filePath, errorMessage);
        if (suffix == "mid")
            return d->openMidiFile(filePath, errorMessage);
        Toast::show(tr("Unrecognized file format: %1").arg(suffix));
        return false;
    }
    Toast::show(tr("File does not exist: %1").arg(filePath));
    return false;
}

bool AppController::saveProject(const QString &filePath, QString &errorMessage) {
    Q_D(AppController);
    if (!appModel->saveProject(filePath, errorMessage))
        return false;

    historyManager->setSavePoint();
    d->updateProjectPathAndName(filePath);
    return true;
}

void AppController::importMidiFile(const QString &filePath) {
    appModel->importMidiFile(filePath);
}

void AppController::exportMidiFile(const QString &filePath) {
    appModel->exportMidiFile(filePath);
}

// void AppController::importAceProject(const QString &filePath) {
//     Q_D(AppController);
//     appModel->importAceProject(filePath);
//     historyManager->reset();
//     historyManager->setSavePoint();
//     d->updateProjectPathAndName("");
//     setProjectName(QFileInfo(filePath).baseName());
//     d->m_lastProjectFolder = QFileInfo(filePath).dir().path();
// }

void AppController::onSetTempo(double tempo) {
    auto model = appModel;
    auto oldTempo = model->tempo();
    auto newTempo = tempo > 0 ? tempo : model->tempo();
    auto actions = new TempoActions;
    actions->editTempo(oldTempo, newTempo, model);
    actions->execute();
    historyManager->record(actions);
}

void AppController::onSetTimeSignature(int numerator, int denominator) {
    Q_D(AppController);
    auto model = appModel;
    auto oldSig = model->timeSignature();
    auto newSig = TimeSignature(numerator, denominator);
    auto actions = new TimeSignatureActions;
    if (d->isPowerOf2(denominator)) {
        actions->editTimeSignature(oldSig, newSig, model);
    } else {
        actions->editTimeSignature(oldSig, oldSig, model);
    }
    actions->execute();
    historyManager->record(actions);
}

void AppController::onSetQuantize(int quantize) {
    appStatus->quantize = quantize;
}

void AppController::setActivePanel(AppGlobal::PanelType panelType) {
    Q_D(AppController);
    for (const auto panel : d->m_panels)
        panel->setPanelActive(panel->panelType() == panelType);
    d->m_activePanel = panelType;
    emit activePanelChanged(panelType);
}

void AppController::onUndoRedoChanged(bool canUndo, const QString &undoActionName, bool canRedo,
                                      const QString &redoActionName) {
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

void AppController::setTrackAndClipPanelCollapsed(bool trackCollapsed, bool clipCollapsed) {
    Q_D(AppController);
    d->m_mainWindow->setTrackAndClipPanelCollapsed(trackCollapsed, clipCollapsed);
}

void AppControllerPrivate::initializeModules() {
    InferEngine::instance();
    InferController::instance();
    ProjectStatusController::instance();
    ValidationController::instance();

    connect(appOptions, &AppOptions::optionsChanged, ThemeManager::instance(),
            &ThemeManager::onAppOptionsChanged);
    connect(appModel, &AppModel::modelChanged, audioDecodingController,
            &AudioDecodingController::onModelChanged);
    connect(appModel, &AppModel::trackChanged, audioDecodingController,
            &AudioDecodingController::onTrackChanged);
    connect(playbackController, &PlaybackController::playbackStatusChanged, taskManager,
            [=](const PlaybackStatus status) {
                if (status == Playing)
                    // 手动触发播放任务
                    taskManager->triggerTimer();
            });
}

bool AppControllerPrivate::isPowerOf2(int num) {
    return num > 0 && ((num & (num - 1)) == 0);
}

void AppControllerPrivate::onRunLanguageEngineTaskFinished(LaunchLanguageEngineTask *task) {
    taskManager->removeTask(task);
    auto status = task->success ? AppStatus::ModuleStatus::Ready : AppStatus::ModuleStatus::Error;
    appStatus->languageModuleStatus = status;
    delete task;
}

void AppControllerPrivate::updateProjectPathAndName(const QString &path) {
    Q_Q(AppController);
    m_projectPath = path;
    q->setProjectName(m_projectPath.isEmpty() ? tr("New Project")
                                              : QFileInfo(m_projectPath).fileName());
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
    return true;
}

bool AppControllerPrivate::openMidiFile(const QString &path, QString &errorMessage) {
    Q_Q(AppController);
    AppModel resultModel;
    constexpr auto midiImport = ImportMode::NewProject;
    if (MidiConverter converter(appModel->timeSignature(), appModel->tempo());
        !converter.load(path, &resultModel, errorMessage, midiImport)) {
        qCritical() << errorMessage;
        return false;
    }

    appModel->loadFromAppModel(resultModel);
    historyManager->reset();
    updateProjectPathAndName("");
    q->setProjectName(QFileInfo(path).baseName());
    m_lastProjectFolder = QFileInfo(path).dir().path();
    return true;
}