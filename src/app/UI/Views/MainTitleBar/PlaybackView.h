//
// Created by fluty on 2024/2/4.
//

#ifndef PLAYBACKVIEW_H
#define PLAYBACKVIEW_H

#include <QSize>
#include <QWidget>

#include "Global/PlaybackGlobal.h"

class QPushButton;
class TempoComboBox;
class TimeSignatureComboBox;
class InlineEditLabel;

using namespace PlaybackGlobal;

class PlaybackView final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor playAccentColor READ playAccentColor WRITE setPlayAccentColor)
    Q_PROPERTY(QColor pauseAccentColor READ pauseAccentColor WRITE setPauseAccentColor)
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
    void updateView();
    void onTempoChanged(double tempo);
    void onTimeSignatureChanged(int numerator, int denominator);
    void onPositionChanged(double tick);
    void onPlaybackStatusChanged(PlaybackStatus status);

protected:
    void changeEvent(QEvent *event) override;

private:
    // Theme color accessors (QSS-overridable via qproperty-*); setters
    // re-tint the already-generated button icons
    [[nodiscard]] QColor playAccentColor() const;
    void setPlayAccentColor(const QColor &color);
    [[nodiscard]] QColor pauseAccentColor() const;
    void setPauseAccentColor(const QColor &color);
    // Re-tint play/pause/loop button icons from the current theme colors
    void rebuildIcons();

    QColor m_playAccentColor = {155, 186, 255};
    QColor m_pauseAccentColor = {255, 205, 155};

    TempoComboBox *m_elTempo;
    QPushButton *m_btnStop;
    TimeSignatureComboBox *m_elTimeSignature;
    QPushButton *m_btnPlay;
    QPushButton *m_btnPause;
    QPushButton *m_btnPlayPause;
    QPushButton *m_btnLoop;
    InlineEditLabel *m_elTime;

    double m_tempo = 120;
    int m_numerator = 4;
    int m_denominator = 4;
    int m_tick = 0;
    PlaybackStatus m_status = Stopped;

    int m_contentHeight = 28;
    QSize m_iconSize = QSize(16, 16);

    QString toFormattedTickTime(int ticks) const;
    int fromTickTimeString(const QStringList &splitStr) const;

    void updateTempoView();
    void updateTimeSignatureView();
    void updateTimeView();
    void updatePlaybackControlView();
    void updateLoopButtonView();
};

#endif // PLAYBACKVIEW_H
