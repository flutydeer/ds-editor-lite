//
// Created by FlutyDeer on 2023/12/1.
//

#include <QFileInfo>
#include <QDir>

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
void AppController::onNewProject() {
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
void AppController::saveProject(const QString &filePath) {
    if (AppModel::instance()->saveProject(filePath)) {
        updateProjectPathAndName(filePath);
        Toast::show(tr("Saved"));
    }
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
    // TODO: validate tempo
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
        // QMessageBox msgBox;
        // msgBox.setText("Error");
        // msgBox.setInformativeText("Denominator error.");
        // msgBox.setStandardButtons(QMessageBox::Yes);
        // msgBox.setDefaultButton(QMessageBox::Yes);
        // msgBox.exec();
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
        m_mainWindow->setProjectName(m_projectName);
}
bool AppController::isLanguageEngineReady() const {
    return m_isLanguageEngineReady;
}
void AppController::registerPanel(IPanel *panel) {
    m_panels.append(panel);
}
