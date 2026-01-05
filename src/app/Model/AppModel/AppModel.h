//
// Created by fluty on 2024/1/27.
//

#ifndef DSPXMODEL_H
#define DSPXMODEL_H

#define appModel AppModel::instance()

#include "Utils/Singleton.h"
#include "Clip.h"
#include "TimeSignature.h"
#include "Interface/ISerializable.h"
#include "TrackControl.h"

class Track;
class WorkspaceEditor;
class AppModelPrivate;

class AppModel final : public QObject, public ISerializable {
    Q_OBJECT

public: // TODO: make it private
    explicit AppModel(QObject *parent = nullptr);
    ~AppModel() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(AppModel)
    Q_DISABLE_COPY_MOVE(AppModel)

public:
    enum TrackChangeType { Insert, Remove };

    TimeSignature timeSignature() const;
    void setTimeSignature(const TimeSignature &signature);
    double tempo() const;
    void setTempo(double tempo);
    TrackControl masterControl() const;
    void setMasterControl(const TrackControl &control);
    const QList<Track *> &tracks() const;
    void insertTrack(Track *track, qsizetype index);
    void appendTrack(Track *track);
    void removeTrackAt(qsizetype index);
    void removeTrack(Track *track);
    void clearTracks();

    //  QJsonObject globalWorkspace() const;
    //  bool isWorkspaceExist(const QString &id) const;
    //  QJsonObject getPrivateWorkspaceById(const QString &id) const;
    // std::unique_ptr<WorkspaceEditor> workspaceEditor(const QString &id);

    void newProject();
    bool importMidiFile(const QString &filename);
    bool exportMidiFile(const QString &filename);
    bool loadProject(const QString &path, QString &errorMessage);
    bool saveProject(const QString &path, QString &errorMessage);
    bool importAceProject(const QString &filename);
    void loadFromAppModel(const AppModel &model);

    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;

    Clip *findClipById(int clipId, Track *&trackRef) const;
    Clip *findClipById(int clipId, int &trackIndex);
    Clip *findClipById(int clipId);
    Track *findTrackById(int id, int &trackIndex);
    Track *findTrackById(int id);
    double tickToMs(double tick) const;
    double msToTick(double ms) const;
    QString getBarBeatTickTime(int ticks) const;
    int projectLengthInTicks() const;

    class LevelMetersUpdatedArgs {
    public:
        class State {
        public:
            double valueL = 0;
            double valueR = 0;
        };

        QList<State> trackMeterStates;
        State metronomeMeterState;
    };

signals:
    void modelChanged();
    void tempoChanged(double tempo);
    void timeSignatureChanged(int numerator, int denominator);
    void masterControlChanged(const TrackControl &control);
    void trackChanged(AppModel::TrackChangeType type, qsizetype index, Track *track);

private:
    Q_DECLARE_PRIVATE(AppModel);
    AppModelPrivate *d_ptr;
};



#endif // DSPXMODEL_H
