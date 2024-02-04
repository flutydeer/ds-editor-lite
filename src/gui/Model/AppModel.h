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

    // class TimeSignature {
    // public:
    //     TimeSignature();
    //     TimeSignature(int numerator, int denominator)
    //         : m_numerator(numerator), m_denominator(denominator) {
    //     }
    //     int numerator() const;
    //     void setNumerator(int numerator);
    //     int denominator() const;
    //     void setDenominator(int denominator);
    //
    // private:
    //     int m_numerator = 4;
    //     int m_denominator = 4;
    // };

    int numerator() const;
    void setNumerator(int numerator);
    int denominator() const;
    void setDenominator(int denominator);
    double tempo() const;
    void setTempo(double tempo);
    const QList<DsTrack *> &tracks() const;
    void insertTrack(DsTrack *track, int index);
    void removeTrack(int index);

    bool importMidi(const QString &filename);
    bool loadAProject(const QString &filename);

    class LevelMetersUpdatedArgs {
    public:
        class State {
        public:
            double valueL = 0;
            double valueR = 0;
            bool clippedL = false;
            bool clippedR = false;
        };
        QList<State> trackMeterStates;
    };

public slots:
    void onTrackUpdated(int index);
    void onSelectedClipChanged(int trackIndex, int clipId);

signals:
    void modelChanged();
    void tempoChanged(double tempo);
    void timeSignatureChanged(int numerator, int denominator);
    void tracksChanged(TrackChangeType type, int index);
    void selectedClipChanged(int trackIndex, int clipIndex);

private:
    void reset();

    int m_numerator = 4;
    int m_denominator = 4;
    double m_tempo = 120;
    QList<DsTrack *> m_tracks;

    // instance
    int m_selectedClipTrackIndex = -1;
    int m_selectedClipId = -1;
};



#endif // DSPXMODEL_H
