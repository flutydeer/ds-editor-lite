//
// Created by fluty on 2024/1/27.
//

#ifndef DSPXMODEL_H
#define DSPXMODEL_H

#define appModel AppModel::instance()

#include <lite/Core/Singleton.h>
#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/MusicBase/Timeline.h>
#include <lite/Support/ISerializable.h>
#include <lite/ProjectModel/AppModel/TrackControl.h>
#include <lite/ProjectModel/AppModel/ProjectModelData.h>

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

    const Timeline &timeline() const;
    void setTimeline(Timeline timeline);
    // Convenience editors for the anchor point at position 0; other timeline
    // points remain untouched.
    void setTempo(double tempo);
    void setTimeSignature(const TimeSignature &signature);
    TrackControl masterControl() const;
    void setMasterControl(const TrackControl &control);
    // Defaults for newly created tracks, supplied by the app layer so the model
    // does not depend on AppOptions / the app-wide color palette.
    void setDefaultSingingLanguage(const QString &language);
    void setPaletteColorCount(int count);
    const QList<Track *> &tracks() const;
    void insertTrack(Track *track, qsizetype index);
    void appendTrack(Track *track);
    // 重排轨道顺序（to 为移动完成后的最终下标，语义同 QList::move）。
    // 与「takeTrackAt + insertTrack」不同：列表先完成重排，之后才发出
    // Remove/Insert 通知，因此监听者在处理 Remove 时仍能通过
    // findTrackById / findClipById 查到该轨道及其剪辑，从而把「移动」
    // 与「删除」区分开，不会误清空 activeClipId、推理管线等状态。
    void moveTrack(qsizetype from, qsizetype to);
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
    void timelineChanged();
    void masterControlChanged(const TrackControl &control);
    void trackChanged(AppModel::TrackChangeType type, qsizetype index, Track *track);

private:
    Q_DECLARE_PRIVATE(AppModel);
    AppModelPrivate *d_ptr;
};

#endif // DSPXMODEL_H
