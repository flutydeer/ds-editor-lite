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
    Q_PROPERTY(QColor actionIconColor READ actionIconColor WRITE setActionIconColor)
    Q_PROPERTY(QColor actionIconDisabledColor READ actionIconDisabledColor WRITE
                   setActionIconDisabledColor)
    Q_PROPERTY(QColor playAccentColor READ playAccentColor WRITE setPlayAccentColor)
    Q_PROPERTY(QColor pauseAccentColor READ pauseAccentColor WRITE setPauseAccentColor)
public:
    explicit PlaybackView(QWidget *parent = nullptr);

signals:
    void setTempoTriggered(int tick, double tempo);
    void setTimeSignatureTriggered(int barIndex, int numerator, int denominator);
    void playTriggered();
    void pauseTriggered();
    void stopTriggered();
    void setPositionTriggered(double tick);

public slots:
    void updateView();
    void onTimelineChanged();
    void onPositionChanged(double tick);
    void onPlaybackStatusChanged(PlaybackStatus status);

protected:
    void changeEvent(QEvent *event) override;

private:
    // Theme color accessors (QSS-overridable via qproperty-*); setters
    // re-tint the already-generated button icons
    [[nodiscard]] QColor actionIconColor() const;
    void setActionIconColor(const QColor &color);
    [[nodiscard]] QColor actionIconDisabledColor() const;
    void setActionIconDisabledColor(const QColor &color);
    [[nodiscard]] QColor playAccentColor() const;
    void setPlayAccentColor(const QColor &color);
    [[nodiscard]] QColor pauseAccentColor() const;
    void setPauseAccentColor(const QColor &color);
    // Re-tint playback button icons from the current theme colors
    void rebuildIcons();

    QColor m_actionIconColor = {240, 240, 240};
    QColor m_actionIconDisabledColor = {240, 240, 240, 102};
    QColor m_playAccentColor = {155, 186, 255};
    QColor m_pauseAccentColor = {255, 205, 155};

    TempoComboBox *m_elTempo = nullptr;
    QPushButton *m_btnStop = nullptr;
    TimeSignatureComboBox *m_elTimeSignature = nullptr;
    QPushButton *m_btnPlay = nullptr;
    QPushButton *m_btnPause = nullptr;
    QPushButton *m_btnPlayPause = nullptr;
    QPushButton *m_btnLoop = nullptr;
    QPushButton *m_btnAutoPageTurn = nullptr;
    InlineEditLabel *m_elTime = nullptr;

    double m_tempo = 120;
    int m_numerator = 4;
    int m_denominator = 4;
    // Bar of the time signature point being edited, snapshotted when editing
    // starts because the playhead keeps moving during playback
    int m_signatureEditBar = 0;
    // Tick of the tempo point being edited, snapshotted for the same reason.
    int m_tempoEditTick = 0;
    int m_tick = 0;
    PlaybackStatus m_status = Stopped;

    int m_contentHeight = 28;
    QSize m_iconSize = QSize(16, 16);

    QString toFormattedTickTime(int ticks) const;
    // Tick of the tempo point governing the playhead position.
    [[nodiscard]] int playheadTempoTick() const;
    // Sync the displayed tempo with the playhead's segment.
    void refreshTempoDisplay();
    // Bar of the point governing the playhead position
    [[nodiscard]] int playheadSignatureBar() const;
    // Sync the displayed time signature with the playhead's segment
    void refreshTimeSignatureDisplay();

    void updateTempoView();
    void updateTimeSignatureView();
    void updateTimeView();
    void updatePlaybackControlView();
    void updateLoopButtonView();
    void updateAutoPageTurnButtonView();
};

#endif // PLAYBACKVIEW_H
