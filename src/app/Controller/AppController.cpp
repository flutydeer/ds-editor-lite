//
// Created by FlutyDeer on 2023/12/1.
//

#include "AppController.h"
#include "AppController_p.h"

#include "AudioDecodingController.h"
#include "Actions/AppModel/Tempo/TempoActions.h"
#include "Actions/AppModel/TimeSignature/TimeSignatureActions.h"
#include "Interface/IMainWindow.h"
#include "Interface/IPanel.h"
#include "Model/Track.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/Task/TaskManager.h"
#include "Tasks/DecodeAudioTask.h"
#include "Tasks/LaunchLanguageEngineTask.h"
#include "UI/Controls/Toast.h"

#include <QDir>
#include <QFileInfo>

AppController::AppController() : d_ptr(new AppControllerPrivate(this)) {
    Q_D(AppController);
    auto task = new LaunchLanguageEngineTask;
    connect(task, &LaunchLanguageEngineTask::finished, this,
            [=] { d->handleRunLanguageEngineTaskFinished(task); });
    TaskManager::instance()->addTask(task);
    TaskManager::instance()->startTask(task);

    connect(AppModel::instance(), &AppModel::modelChanged, AudioDecodingController::instance(),
            &AudioDecodingController::onModelChanged);
    connect(AppModel::instance(), &AppModel::trackChanged, AudioDecodingController::instance(),
            &AudioDecodingController::onTrackChanged);
}
AppController::~AppController() {
    delete d_ptr;
}
void AppController::newProject() {
    Q_D(AppController);
    AppModel::instance()->newProject();
    HistoryManager::instance()->reset();
    d->updateProjectPathAndName("");
}
void AppController::openProject(const QString &filePath) {
    Q_D(AppController);
    AppModel::instance()->loadProject(filePath);
    HistoryManager::instance()->reset();
    d->updateProjectPathAndName(filePath);
    d->m_lastProjectFolder = QFileInfo(filePath).dir().path();
}
bool AppController::saveProject(const QString &filePath) {
    Q_D(AppController);
    if (AppModel::instance()->saveProject(filePath)) {
        HistoryManager::instance()->setSavePoint();
        d->updateProjectPathAndName(filePath);
        Toast::show(tr("Saved"));
        return true;
    }
    Toast::show(tr("Failed to save project"));
    return false;
}
void AppController::importMidiFile(const QString &filePath) {
    AppModel::instance()->importMidiFile(filePath);
}
void AppController::exportMidiFile(const QString &filePath) {
    AppModel::instance()->exportMidiFile(filePath);
}
void AppController::importAproject(const QString &filePath) {
    Q_D(AppController);
    AppModel::instance()->importAProject(filePath);
    HistoryManager::instance()->reset();
    d->updateProjectPathAndName("");
    setProjectName(QFileInfo(filePath).baseName());
    d->m_lastProjectFolder = QFileInfo(filePath).dir().path();
}
void AppController::onSetTempo(double tempo) {
    auto model = AppModel::instance();
    auto oldTempo = model->tempo();
    auto newTempo = tempo > 0 ? tempo : model->tempo();
    auto actions = new TempoActions;
    actions->editTempo(oldTempo, newTempo, model);
    actions->execute();
    HistoryManager::instance()->record(actions);
}
void AppController::onSetTimeSignature(int numerator, int denominator) {
    Q_D(AppController);
    auto model = AppModel::instance();
    auto oldSig = model->timeSignature();
    auto newSig = AppModel::TimeSignature(numerator, denominator);
    auto actions = new TimeSignatureActions;
    if (d->isPowerOf2(denominator)) {
        actions->editTimeSignature(oldSig, newSig, model);
    } else {
        actions->editTimeSignature(oldSig, oldSig, model);
    }
    actions->execute();
    HistoryManager::instance()->record(actions);
}
void AppController::onSetQuantize(int quantize) {
    AppModel::instance()->setQuantize(quantize);
}
void AppController::onTrackSelectionChanged(int trackIndex) {
    AppModel::instance()->setSelectedTrack(trackIndex);
}
void AppController::onPanelClicked(AppGlobal::PanelType panelType) {
    Q_D(AppController);
    for (const auto panel : d->m_panels)
        panel->setPanelActivated(panel->panelType() == panelType);
    d->m_activatedPanel = panelType;
    emit activatedPanelChanged(panelType);
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
bool AppController::isLanguageEngineReady() const {
    Q_D(const AppController);
    return d->m_isLanguageEngineReady;
}
void AppController::registerPanel(IPanel *panel) {
    Q_D(AppController);
    d->m_panels.append(panel);
}
bool AppControllerPrivate::isPowerOf2(int num) {
    return num > 0 && ((num & (num - 1)) == 0);
}
void AppControllerPrivate::handleRunLanguageEngineTaskFinished(LaunchLanguageEngineTask *task) {
    qDebug() << "AppController::handleRunLanguageEngineTaskFinished";
    TaskManager::instance()->removeTask(task);
    m_isLanguageEngineReady = task->success;
    delete task;
}
void AppControllerPrivate::updateProjectPathAndName(const QString &path) {
    Q_Q(AppController);
    m_projectPath = path;
    q->setProjectName(m_projectPath.isEmpty() ? q->tr("New Project")
                                           : QFileInfo(m_projectPath).fileName());
}