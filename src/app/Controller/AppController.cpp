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
#include "Modules/Task/TaskManager.h"
#include "Tasks/DecodeAudioTask.h"
#include "Tasks/LaunchLanguageEngineTask.h"
#include "UI/Utils/ThemeManager.h"
#include "Utils/Log.h"

#include "Actions/AppModel/MasterControl/MasterControlActions.h"

AppController::AppController(QObject *parent)
    : QObject(parent), d_ptr(new AppControllerPrivate(this)) {
    Q_D(AppController);
    AppControllerPrivate::initializeModules();
}

AppController::~AppController() {
    delete d_ptr;
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(AppController)

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

void AppControllerPrivate::onRunLanguageEngineTaskFinished(LaunchLanguageEngineTask *task) {
    taskManager->removeTask(task);
    const auto status =
        task->success ? AppStatus::ModuleStatus::Ready : AppStatus::ModuleStatus::Error;
    appStatus->languageModuleStatus = status;
    delete task;
}
