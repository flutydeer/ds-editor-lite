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

    enum TrackChangeType { Insert, PropertyUpdate, Remove };
    int numerator = 4;
    int denominator = 4;

    double tempo() const;
    void setTempo(double tempo);
    const QList<DsTrack *> &tracks() const;
    void insertTrack(DsTrack *track, int index);
    void removeTrack(int index);

    bool importMidi(const QString &filename);
    bool loadAProject(const QString &filename);

public slots:
    void onTrackUpdated(int index);
    void onSelectedClipChanged(int trackIndex, int clipIndex);

signals:
    void modelChanged();
    void tempoChanged(double tempo);
    void timeSignatureChanged(int numerator, int denominator);
    void tracksChanged(TrackChangeType type, int index);
    void selectedClipChanged(int trackIndex, int clipIndex);

private:
    void reset();

    double m_tempo = 120;
    QList<DsTrack *> m_tracks;

    // instance
    int m_selectedClipTrackIndex = -1;
    int m_selectedClipIndex = -1;
};



#endif // DSPXMODEL_H
