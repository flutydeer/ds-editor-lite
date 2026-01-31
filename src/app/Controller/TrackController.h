//
// Created by fluty on 2024/1/31.
//

#ifndef TRACKCONTROLLER_H
#define TRACKCONTROLLER_H

#define trackController TrackController::instance()

#include "Model/AppModel/Clip.h"
#include "Model/AppModel/Track.h"
#include "Tasks/DecodeAudioTask.h"
#include "Utils/Singleton.h"

#include <QObject>


class SingingClip;
class QWidget;

namespace talcs {
    class AbstractAudioFormatIO;
}

class TrackController final : public QObject {
    Q_OBJECT

private:
    explicit TrackController(QObject *parent = nullptr);
    ~TrackController() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(TrackController)
    Q_DISABLE_COPY_MOVE(TrackController)

public:
    void setParentWidget(QWidget *view);

public slots:
    static void onNewTrack();
    static void onInsertNewTrack(qsizetype index);
    static void onAppendTrack(Track *track);
    static void onRemoveTrack(int id);
    static void onMoveTrack(qsizetype fromIndex, qsizetype toIndex);
    static void addAudioClipToNewTrack(const QString &filePath);
    static void setActiveClip(int clipId);
    static void changeTrackProperty(const Track::TrackProperties &args);
    void onAddAudioClip(const QString &path, talcs::AbstractAudioFormatIO *io, const QJsonObject &workspace, int id, int tick);
    static void onClipPropertyChanged(const Clip::ClipCommonProperties &args);
    static void onRemoveClips(const QList<int> &clipsId);
    static SingingClip *onNewSingingClip(int trackIndex, int tick);

private:
    void handleDecodeAudioTaskFinished(DecodeAudioTask *task);

    // TODO: refactor
    QWidget *m_parentWidget = nullptr;
};

#endif // TRACKCONTROLLER_H
