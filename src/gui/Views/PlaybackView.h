//
// Created by fluty on 2024/2/4.
//

#ifndef PLAYBACKVIEW_H
#define PLAYBACKVIEW_H

#include <QWidget>
#include <QPushButton>

#include "./Controller/PlaybackController.h"
#include "Controls/Base/EditLabel.h"

class PlaybackView final : public QWidget {
    Q_OBJECT
public:
    explicit PlaybackView(QWidget *parent = nullptr);

signals:
    void changeTempoTriggered(double tempo);

public slots:
    void onTempoChanged(double tempo);
    void onTimeSignatureChanged(int numerator, int denominator);
    // void onPositionChanged(double tick);
    void onPlaybackStatusChanged(PlaybackController::PlaybackStatus status);

private:
    EditLabel *m_elTempo;
    EditLabel *m_elTimeSignature;
    QPushButton *m_btnStop;
    QPushButton *m_btnPlayPause;
    // QPushButton *m_btnLoop;
    EditLabel *m_elTime;

    double m_tempo = 120;
    int m_numerator = 4;
    int m_denominator = 4;
    int m_tick = 0;

    int m_contentHeight = 28;

    QString toFormattedTickTime(int ticks);
    // double fromTickTimeString(const QString &str);
};



#endif // PLAYBACKVIEW_H
