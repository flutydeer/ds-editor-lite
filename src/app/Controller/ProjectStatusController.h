//
// Created by fluty on 24-8-30.
//

#ifndef PROJECTSTATUSCONTROLLER_H
#define PROJECTSTATUSCONTROLLER_H

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Track.h"
#include "Utils/Singleton.h"

#include <QObject>

class ProjectStatusController : public QObject, public Singleton<ProjectStatusController> {
    Q_OBJECT

public:
    explicit ProjectStatusController();

private slots:
    void onModelChanged();
    void onTrackChanged(AppModel::TrackChangeType type, qsizetype index, Track *track);
    void onClipChanged(Track::ClipChangeType type, Clip *clip);

private:
    void handleClipInserted(Clip *clip);
    void handleClipRemoved(Clip *clip) const;
    static void updateProjectEditableLength();

    QList<Track *> m_tracks;
};



#endif // PROJECTSTATUSCONTROLLER_H
