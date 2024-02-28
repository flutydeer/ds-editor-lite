//
// Created by fluty on 2024/1/29.
//

#ifndef TRACKCONTROLWIDGET_H
#define TRACKCONTROLWIDGET_H

#include <QWidget>

#include "Model/TrackControl.h"

class QListWidgetItem;
class LevelMeter;
class Track;
class QPushButton;
class QLabel;
class EditLabel;
class SeekBar;
class QSpacerItem;
class QHBoxLayout;
class QVBoxLayout;

class TrackControlWidget final : public QWidget {
    Q_OBJECT

public:
    explicit TrackControlWidget(QListWidgetItem *item, QWidget *parent = nullptr);
    [[nodiscard]] int trackIndex() const;
    void setTrackIndex(int i);
    [[nodiscard]] QString name() const;
    void setName(const QString &name);
    [[nodiscard]] TrackControl control() const;
    void setControl(const TrackControl &control);
    void setNarrowMode(bool on);
    [[nodiscard]] LevelMeter *levelMeter() const;

signals:
    void propertyChanged();
    // void propertyChanged(const QString &name, const QString &value);
    void insertNewTrackTriggered();
    void removeTrackTriggerd();
    // void moveUpTrack();
    // void modeDownTrack();
    void addAudioClipTriggered();

public slots:
    void onTrackUpdated(const Track &track);
    // void setScale(qreal sx, qreal sy);
    // void setHeight(int h);

private slots:
    void onSeekBarValueChanged();

private:
    void contextMenuEvent(QContextMenuEvent *event) override;
    QListWidgetItem *m_item;
    // controls
    QPushButton *m_btnColor;
    QLabel *m_lbTrackIndex;
    QPushButton *m_btnMute;
    QPushButton *m_btnSolo;
    EditLabel *m_leTrackName;
    SeekBar *m_sbarPan;
    EditLabel *m_lePan;
    SeekBar *m_sbarGain;
    EditLabel *m_leGain;
    QSpacerItem *m_panVolumeSpacer;
    LevelMeter *m_levelMeter;
    QHBoxLayout *m_mainLayout;
    QVBoxLayout *m_controlWidgetLayout;
    QHBoxLayout *m_muteSoloTrackNameLayout;
    QHBoxLayout *m_panVolumeLayout;

    int m_buttonSize = 24;

    static QString panValueToString(double value);
    static QString gainValueToString(double value);
};



#endif // TRACKCONTROLWIDGET_H
