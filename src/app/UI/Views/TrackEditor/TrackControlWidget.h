//
// Created by fluty on 2024/1/29.
//

#ifndef TRACKCONTROLWIDGET_H
#define TRACKCONTROLWIDGET_H

#include "Global/AppGlobal.h"
#include "Interface/ITrack.h"
#include "Model/TrackControl.h"

#include <QWidget>

class LanguageComboBox;
class QListWidgetItem;
class LevelMeter;
class Track;
class Button;
class QLabel;
class EditLabel;
class SeekBar;
class QSpacerItem;
class QHBoxLayout;
class QVBoxLayout;

class TrackControlWidget final : public QWidget, public ITrack {
    Q_OBJECT

public:
    explicit TrackControlWidget(QListWidgetItem *item, int id, QWidget *parent = nullptr);
    [[nodiscard]] int trackIndex() const;
    void setTrackIndex(int i);
    [[nodiscard]] QString name() const override;
    void setName(const QString &name) override;
    [[nodiscard]] TrackControl control() const override;
    void setControl(const TrackControl &control) override;
    void setNarrowMode(bool on);
    void setLanguage(AppGlobal::languageType lang);
    [[nodiscard]] LevelMeter *levelMeter() const;
    [[nodiscard]] QColor color() const override {
        return {};
    }
    void setColor(const QColor &color) override {
    }

signals:
    void insertNewTrackTriggered();
    void removeTrackTriggered(int id);
    // void moveUpTrack();
    // void modeDownTrack();
    // void addAudioClipTriggered();

public slots:
    void onTrackUpdated(const Track &track);
    // void setScale(qreal sx, qreal sy);
    // void setHeight(int h);

private slots:
    void onSeekBarValueChanged();

private:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void changeTrackProperty();
    QListWidgetItem *m_item;
    // controls
    // Button *m_btnColor;
    QLabel *m_lbTrackIndex;
    Button *m_btnMute;
    Button *m_btnSolo;
    EditLabel *m_leTrackName;
    LanguageComboBox *m_cbLanguage;
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
