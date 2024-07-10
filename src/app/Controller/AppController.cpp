//
// Created by FlutyDeer on 2023/12/1.
//

#include "AppController.h"
#include "Modules/History/HistoryManager.h"
#include "Actions/AppModel/Tempo/TempoActions.h"
#include "Actions/AppModel/TimeSignature/TimeSignatureActions.h"
#include "Interface/IPanel.h"
#include "Model/Clip.h"
#include "Model/Track.h"
#include "Tasks/DecodeAudioTask.h"
#include "Modules/Task/TaskManager.h"
#include "Tasks/LaunchLanguageEngineTask.h"

AppController::AppController() {
    auto task = new LaunchLanguageEngineTask;
    connect(task, &LaunchLanguageEngineTask::finished, this,
            [=] { handleRunLanguageEngineTaskFinished(task); });
    TaskManager::instance()->addTask(task);
    TaskManager::instance()->startTask(task);
}
void AppController::onNewProject() {
    AppModel::instance()->newProject();
    HistoryManager::instance()->reset();
    m_lastProjectPath = "";
}
void AppController::openProject(const QString &filePath) {
    AppModel::instance()->loadProject(filePath);
    HistoryManager::instance()->reset();
    m_lastProjectPath = filePath;
}
void AppController::saveProject(const QString &filePath) {
    if (AppModel::instance()->saveProject(filePath)) {
        m_lastProjectPath = filePath;
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
    m_lastProjectPath = "";
    decodeAllAudioClips(*AppModel::instance());
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
void AppController::decodeAllAudioClips(AppModel &model) {
    for (auto track : model.tracks()) {
        for (auto clip : track->clips()) {
            if (clip->type() == Clip::Audio)
                createAndStartDecodeAudioTask(reinterpret_cast<AudioClip *>(clip));
        }
    }
    // TaskManager::instance()->startAllTasks();
}
void AppController::createAndStartDecodeAudioTask(AudioClip *clip) {
    auto decodeTask = new DecodeAudioTask(clip->id());
    decodeTask->path = clip->path();
    connect(decodeTask, &ITask::finished, this,
            [=](bool terminate) { handleDecodeAudioTaskFinished(decodeTask, terminate); });
    TaskManager::instance()->addTask(decodeTask);
    TaskManager::instance()->startTask(decodeTask);
}
void AppController::handleDecodeAudioTaskFinished(DecodeAudioTask *task, bool terminate) {
    TaskManager::instance()->removeTask(task);
    if (terminate)
        return;

    int trackIndex;
    auto clip = AppModel::instance()->findClipById(task->id(), trackIndex);
    if(!clip)
        return;

    if (clip->type() == Clip::Audio) {
        AudioInfoModel info;
        info.sampleRate = task->sampleRate;
        info.channels = task->channels;
        info.chunkSize = task->chunkSize;
        info.mipmapScale = task->mipmapScale;
        info.frames = task->frames;
        info.peakCache.swap(task->peakCache);
        info.peakCacheMipmap.swap(task->peakCacheMipmap);

        auto audioClip = reinterpret_cast<AudioClip *>(clip);
        audioClip->info = info;
        auto track = AppModel::instance()->tracks().at(trackIndex);
        track->notityClipPropertyChanged(audioClip);
    }
}
void AppController::handleRunLanguageEngineTaskFinished(LaunchLanguageEngineTask *task) {
    qDebug() << "AppController::handleRunLanguageEngineTaskFinished";
    TaskManager::instance()->removeTask(task);
    m_isLanguageEngineReady = task->success;
    delete task;
}

QString AppController::lastProjectPath() const {
    return m_lastProjectPath;
}
bool AppController::isLanguageEngineReady() const {
    return m_isLanguageEngineReady;
}
void AppController::registerPanel(IPanel *panel) {
    m_panels.append(panel);
}
