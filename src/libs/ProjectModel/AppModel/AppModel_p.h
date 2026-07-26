//
// Created by fluty on 24-7-28.
//

#ifndef APPMODEL_P_H
#define APPMODEL_P_H

#include <QString>

class AppModel;

class AppModelPrivate {
    Q_DECLARE_PUBLIC(AppModel)

public:
    explicit AppModelPrivate(AppModel *q) : q_ptr(q) {
    }

    void reset();
    void dispose();

    TimeSignature m_timeSignature;
    double m_tempo = 120;
    TrackControl m_masterControl;
    QList<Track *> m_tracks;
    QList<Track *> m_previousTracks;
    QJsonObject m_workspace;

    int m_selectedTrackIndex = -1;
    int m_activeClipId = -1;

    int m_quantize = 16;

    // App-provided defaults for new tracks, pushed in by the app layer so the
    // model does not reach into AppOptions / the app-wide palette itself.
    QString m_defaultSingingLanguage = QStringLiteral("unknown");
    int m_paletteColorCount = 12;

private:
    AppModel *q_ptr;
};

#endif // APPMODEL_P_H
