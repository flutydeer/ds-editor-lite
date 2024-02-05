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
    void setTempoTriggered(double tempo);
    void setTimeSignatureTriggered(int numerator, int denominator);
    void playTriggered();
    void pauseTriggered();
    void stopTriggered();
    void setPositionTriggered(double tick);

public slots:
    void onTempoChanged(double tempo);
    void onTimeSignatureChanged(int numerator, int denominator);
    void onPositionChanged(double tick);
    void onPlaybackStatusChanged(PlaybackController::PlaybackStatus status);

private:
    EditLabel *m_elTempo;
    EditLabel *m_elTimeSignature;
    QPushButton *m_btnStop;
    QPushButton *m_btnPlay;
    QPushButton *m_btnPause;
    // QPushButton *m_btnLoop;
    EditLabel *m_elTime;

    double m_tempo = 120;
    int m_numerator = 4;
    int m_denominator = 4;
    int m_tick = 0;

    int m_contentHeight = 32;

    QString toFormattedTickTime(int ticks);
    int fromTickTimeString(const QStringList &splitStr);

    const QIcon icoPlayWhite = QIcon(":svg/icons/play_16_filled_white.svg");
    const QIcon icoPlayBlack = QIcon(":svg/icons/play_16_filled.svg");
    const QIcon icoPauseWhite = QIcon(":svg/icons/pause_16_filled_white.svg");
    const QIcon icoPauseBlack = QIcon(":svg/icons/pause_16_filled.svg");
    const QIcon icoStopWhite = QIcon(":svg/icons/stop_16_filled_white.svg");
    const QIcon icoStopBlack = QIcon(":svg/icons/stop_16_filled.svg");

    void updateTimeView();
};



#endif // PLAYBACKVIEW_H
