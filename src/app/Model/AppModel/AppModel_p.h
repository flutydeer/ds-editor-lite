//
// Created by fluty on 24-7-28.
//

#ifndef APPMODEL_P_H
#define APPMODEL_P_H

class AppModel;

class AppModelPrivate {
    Q_DECLARE_PUBLIC(AppModel)

public:
    explicit AppModelPrivate(AppModel *q) : q_ptr(q){};

    void reset();

    TimeSignature m_timeSignature;
    double m_tempo = 120;
    QList<Track *> m_tracks;
    QJsonObject m_workspace;

    int m_selectedTrackIndex = -1;
    int m_activeClipId = -1;

    int m_quantize = 16;

private:
    AppModel *q_ptr;
};

#endif // APPMODEL_P_H
