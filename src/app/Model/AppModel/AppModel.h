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
#include "ProjectModelData.h"

class Track;
class WorkspaceEditor;
class AppModelPrivate;

class AppModel final : public QObject, public ISerializable {
    Q_OBJECT

public:
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
    Track *takeTrackAt(qsizetype index);
    Track *takeTrack(Track *track);
    void clearTracks();
    ProjectModelData takeProjectData();
    void replaceProject(ProjectModelData &&data);

public slots:
    void newProject();
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
