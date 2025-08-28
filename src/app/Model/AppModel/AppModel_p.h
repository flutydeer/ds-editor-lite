//
// Created by fluty on 24-7-28.
//

#ifndef APPMODEL_P_H
#define APPMODEL_P_H

class AppModel;

class AppModelPrivate {
    Q_DECLARE_PUBLIC(AppModel)

public:
    explicit AppModelPrivate(AppModel *q) : q_ptr(q) {
    }

    void reset();
    void dispose() const;

    TimeSignature m_timeSignature;
    double m_tempo = 120;
    TrackControl m_masterControl;
    QList<Track *> m_tracks;
    QList<Track *> m_previousTracks;
    QJsonObject m_workspace;

    int m_selectedTrackIndex = -1;
    int m_activeClipId = -1;

    int m_quantize = 16;

private:
    AppModel *q_ptr;
};

#endif // APPMODEL_P_H
