//
// Created by FlutyDeer on 2025/3/25.
//

#ifndef CHANNELVIEW_H
#define CHANNELVIEW_H

#include "Interface/ITrack.h"

#include <QWidget>

class PanSlider;
class Track;
class TrackControl;
class Button;
class QVBoxLayout;
class QHBoxLayout;
class QStackedWidget;
class QLabel;
class EditLabel;
class Fader;
class LevelMeter;

class ChannelView : public QWidget, public ITrack {
    Q_OBJECT

public:
    // enum ChannelType {
    //     Track,
    //     Master
    // };

    explicit ChannelView(QWidget *parent = nullptr);
    explicit ChannelView(Track &track, QWidget *parent = nullptr);

    Track &context() const;
    void setIsMasterChannel(bool on);
    TrackControl control() const override;
    QString name() const override;
    QColor color() const override;
    void setColor(const QColor &color) override;

    PanSlider *const &panSlider() const;
    Fader *const &fader() const;
    LevelMeter *const &levelMeter() const;

public slots:
    void setName(const QString &name) override;
    void setChannelIndex(int index) const;
    void setControl(const TrackControl &control) override;

signals:
    void controlChanged(const TrackControl &control);

private:
    void onPanMoved(double pan) const;
    void onPanReleased(double pan);
    void onPanEdited(const QString &text) const;

    void onFaderMoved(double gain) const;
    void onFaderReleased(double gain);
    void onGainEdited(const QString &text) const;
    void onPeakChanged(double peak) const;

    void initUi();
    static QString gainValueToString(double gain);
    static QString panValueToString(double pan);
    static double panValueFromString(const QString &panStr);
    QVBoxLayout *buildPanSliderLayout();
    QHBoxLayout *buildFaderLevelMeterLayout();
    QVBoxLayout *buildChannelContentLayout();
    QStackedWidget *buildMuteSoloStack();
    QStackedWidget *buildIndexStack();

    Track *m_context = nullptr;
    bool m_isMasterChannel = false;
    bool m_notifyBarrier = false;

    PanSlider *m_panSlider = nullptr;
    EditLabel *m_elPan = nullptr;

    Fader *m_fader = nullptr;
    EditLabel *m_elGain = nullptr;

    LevelMeter *m_levelMeter = nullptr;
    QLabel *m_lbPeakLevel = nullptr;

    Button *m_btnMute = nullptr;
    Button *m_btnSolo = nullptr;

    QLabel *m_lbTitle = nullptr;
    QLabel *m_lbIndex = nullptr;

    QStackedWidget *m_muteSoloStack = nullptr;
    QStackedWidget *m_indexStack = nullptr;
};

#endif // CHANNELVIEW_H