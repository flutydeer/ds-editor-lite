//
// Created by fluty on 2024/1/29.
//

#ifndef TRACKCONTROLWIDGET_H
#define TRACKCONTROLWIDGET_H

#include "Global/AppGlobal.h"
#include "Interface/ITrack.h"
#include "Model/AppModel/TrackControl.h"
#include "UI/Controls/TwoLevelComboBox.h"

#include <QWidget>

namespace SVS {
    class SeekBar;
}
class LanguageComboBox;
class QListWidgetItem;
class LevelMeter;
class Track;
class Button;
class QLabel;
class EditLabel;
class QSpacerItem;
class QHBoxLayout;
class QVBoxLayout;
class ComboBox;

class TrackControlView final : public QWidget, public ITrack {
    Q_OBJECT

public:
    explicit TrackControlView(QListWidgetItem *item, Track *track, QWidget *parent = nullptr);
    [[nodiscard]] int trackIndex() const;
    void setTrackIndex(int i) const;
    [[nodiscard]] QString name() const override;
    void setName(const QString &name) override;
    [[nodiscard]] TrackControl control() const override;
    void setControl(const TrackControl &control) override;
    void setNarrowMode(bool on) const;
    void setLanguage(const QString &language) const;
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

private:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void changeTrackProperty() const;
    bool m_notifyBarrier = false;
    Track *m_track = nullptr;
    TrackControl m_control;
    QListWidgetItem *m_item;
    // controls
    QLabel *lbTrackIndex;
    Button *btnMute;
    Button *btnSolo;
    EditLabel *leTrackName;
    TwoLevelComboBox *cbSinger;
    LanguageComboBox *cbLanguage;
    QSpacerItem *panVolumeSpacer;
    LevelMeter *m_levelMeter;
    QHBoxLayout *mainLayout;
    QVBoxLayout *controlWidgetLayout;
    QHBoxLayout *muteSoloTrackNameLayout;
    QHBoxLayout *singerLanguageLayout;

    int m_buttonSize = 24;

    static QString panValueToString(double value);
    static QString gainValueToString(double value);
    static double gainFromSliderValue(double value);
    static double gainToSliderValue(double gain);
};



#endif // TRACKCONTROLWIDGET_H
