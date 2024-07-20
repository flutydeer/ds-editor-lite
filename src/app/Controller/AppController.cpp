//
// Created by FlutyDeer on 2023/12/1.
//

#include "AppController.h"

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

AppController::AppController() {
    auto task = new LaunchLanguageEngineTask;
    connect(task, &LaunchLanguageEngineTask::finished, this,
            [=] { handleRunLanguageEngineTaskFinished(task); });
    TaskManager::instance()->addTask(task);
    TaskManager::instance()->startTask(task);

    connect(AppModel::instance(), &AppModel::modelChanged, AudioDecodingController::instance(),
            &AudioDecodingController::onModelChanged);
    connect(AppModel::instance(), &AppModel::trackChanged, AudioDecodingController::instance(),
            &AudioDecodingController::onTrackChanged);
}
void AppController::newProject() {
    AppModel::instance()->newProject();
    HistoryManager::instance()->reset();
    updateProjectPathAndName("");
}
void AppController::openProject(const QString &filePath) {
    AppModel::instance()->loadProject(filePath);
    HistoryManager::instance()->reset();
    updateProjectPathAndName(filePath);
    m_lastProjectFolder = QFileInfo(filePath).dir().path();
}
bool AppController::saveProject(const QString &filePath) {
    if (AppModel::instance()->saveProject(filePath)) {
        HistoryManager::instance()->setSavePoint();
        updateProjectPathAndName(filePath);
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
    AppModel::instance()->importAProject(filePath);
    HistoryManager::instance()->reset();
    updateProjectPathAndName("");
    setProjectName(QFileInfo(filePath).baseName());
    m_lastProjectFolder = QFileInfo(filePath).dir().path();
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
    auto model = AppModel::instance();
    auto oldSig = model->timeSignature();
    auto newSig = AppModel::TimeSignature(numerator, denominator);
    auto actions = new TimeSignatureActions;
    if (isPowerOf2(denominator)) {
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
    for (const auto panel : m_panels)
        panel->setPanelActivated(panel->panelType() == panelType);
    m_activatedPanel = panelType;
    emit activatedPanelChanged(panelType);
}
void AppController::onUndoRedoChanged(bool canUndo, const QString &undoActionName, bool canRedo,
                                      const QString &redoActionName) {
    Q_UNUSED(canUndo);
    Q_UNUSED(canRedo);
    Q_UNUSED(redoActionName);
    Q_UNUSED(undoActionName);
    m_mainWindow->updateWindowTitle();
}
bool AppController::isPowerOf2(int num) {
    return num > 0 && ((num & (num - 1)) == 0);
}
void AppController::handleRunLanguageEngineTaskFinished(LaunchLanguageEngineTask *task) {
    qDebug() << "AppController::handleRunLanguageEngineTaskFinished";
    TaskManager::instance()->removeTask(task);
    m_isLanguageEngineReady = task->success;
    delete task;
}
void AppController::updateProjectPathAndName(const QString &path) {
    m_projectPath = path;
    setProjectName(m_projectPath.isEmpty() ? defaultProjectName
                                           : QFileInfo(m_projectPath).fileName());
}

void AppController::setMainWindow(IMainWindow *window) {
    m_mainWindow = window;
}
QString AppController::lastProjectFolder() const {
    return m_lastProjectFolder;
}
QString AppController::projectPath() const {
    return m_projectPath;
}
QString AppController::projectName() const {
    return m_projectName;
}
void AppController::setProjectName(const QString &name) {
    m_projectName = name;
    if (m_mainWindow)
        m_mainWindow->updateWindowTitle();
}
bool AppController::isLanguageEngineReady() const {
    return m_isLanguageEngineReady;
}
void AppController::registerPanel(IPanel *panel) {
    m_panels.append(panel);
}
