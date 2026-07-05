#include "LevelMeterManager.h"
#include "LevelMeterViewModel.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Track.h"

LevelMeterManager::LevelMeterManager(AppModel *model, QObject *parent)
    : QObject(parent), m_appModel(model) {

    syncAll();

    connect(m_appModel, &AppModel::modelChanged, this, &LevelMeterManager::onModelChanged);
    connect(m_appModel, &AppModel::trackChanged, this,
            [this](AppModel::TrackChangeType type, qsizetype index, Track *track) {
                onTrackChanged(static_cast<int>(type), static_cast<int>(index), track);
            });
}

LevelMeterViewModel *LevelMeterManager::viewModelForTrack(const Track *track) const {
    const auto index = m_appModel->tracks().indexOf(const_cast<Track *>(track));
    if (index < 0 || index >= m_viewModels.size())
        return nullptr;
    return m_viewModels.at(index);
}

LevelMeterViewModel *LevelMeterManager::viewModelAt(int index) const {
    if (index < 0 || index >= m_viewModels.size())
        return nullptr;
    return m_viewModels.at(index);
}

LevelMeterViewModel *LevelMeterManager::masterViewModel() const {
    return m_masterViewModel;
}

void LevelMeterManager::clearAllClipStates() {
    for (auto vm : m_viewModels)
        vm->resetClip();
    if (m_masterViewModel)
        m_masterViewModel->resetClip();
}

void LevelMeterManager::onModelChanged() {
    clearAll();
    syncAll();
}

void LevelMeterManager::onTrackChanged(int type, int index, Track * /*track*/) {
    if (type == static_cast<int>(AppModel::Insert))
        insertModel(index);
    else if (type == static_cast<int>(AppModel::Remove))
        removeModelAt(index);
}

void LevelMeterManager::syncAll() {
    for (int i = 0; i < m_appModel->tracks().size(); i++)
        m_viewModels.append(new LevelMeterViewModel(this));
    m_masterViewModel = new LevelMeterViewModel(this);
}

void LevelMeterManager::insertModel(int index) {
    m_viewModels.insert(index, new LevelMeterViewModel(this));
}

void LevelMeterManager::removeModelAt(int index) {
    if (index < 0 || index >= m_viewModels.size())
        return;
    delete m_viewModels.takeAt(index);
}

void LevelMeterManager::clearAll() {
    qDeleteAll(m_viewModels);
    m_viewModels.clear();
    delete m_masterViewModel;
    m_masterViewModel = nullptr;
}