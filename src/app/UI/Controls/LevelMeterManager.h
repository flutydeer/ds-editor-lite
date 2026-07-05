#ifndef LEVELMETERMANAGER_H
#define LEVELMETERMANAGER_H

#include <QObject>
#include <QList>

class AppModel;
class Track;
class LevelMeterViewModel;

class LevelMeterManager : public QObject {
    Q_OBJECT

public:
    explicit LevelMeterManager(AppModel *model, QObject *parent = nullptr);

    LevelMeterViewModel *viewModelForTrack(const Track *track) const;
    LevelMeterViewModel *viewModelAt(int index) const;
    LevelMeterViewModel *masterViewModel() const;

    void clearAllClipStates();

private:
    void onTrackChanged(int type, int index, Track *track);
    void onModelChanged();
    void syncAll();
    void insertModel(int index);
    void removeModelAt(int index);
    void clearAll();

    AppModel *m_appModel = nullptr;
    QList<LevelMeterViewModel *> m_viewModels;
    LevelMeterViewModel *m_masterViewModel = nullptr;
};

#endif // LEVELMETERMANAGER_H