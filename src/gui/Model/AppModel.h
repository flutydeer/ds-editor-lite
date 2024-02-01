//
// Created by fluty on 2024/1/27.
//

#ifndef DSPXMODEL_H
#define DSPXMODEL_H

#include "DsTrack.h"
#include "Utils/Singleton.h"

class AppModel final : public QObject, public Singleton<AppModel> {
    Q_OBJECT

public:
    explicit AppModel() = default;

    enum TrackChangeType { Insert, Update, Remove };
    int numerator = 4;
    int denominator = 4;

    double tempo() const;
    void setTempo(double tempo);
    QList<DsTrack *> tracks() const;
    void insertTrack(DsTrack *track, int index);
    void removeTrack(int index);

    bool loadAProject(const QString &filename);

public slots:
    void onTrackUpdated(int index);
    void onSelectedClipChanged(int trackIndex, int clipIndex);

signals:
    void modelChanged(const AppModel &model);
    void tempoChanged(double tempo);
    void tracksChanged(TrackChangeType type, const AppModel &model, int index);
    void selectedClipChanged(const AppModel &model, int trackIndex, int clipIndex);

private:
    void reset();
    void runG2p();

    double m_tempo = 120;
    QList<DsTrack *> m_tracks;

    // instance
    int m_selectedClipTrackIndex = -1;
    int m_selectedClipIndex = -1;
};



#endif // DSPXMODEL_H
