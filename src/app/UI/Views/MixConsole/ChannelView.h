//
// Created by FlutyDeer on 2025/3/25.
//

#ifndef CHANNELVIEW_H
#define CHANNELVIEW_H

#include "Interface/ITrack.h"

#include <QWidget>

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
    [[nodiscard]] TrackControl control() const override;
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QColor color() const override;
    void setColor(const QColor &color) override;

    [[nodiscard]] Fader *const &fader() const;
    [[nodiscard]] LevelMeter *const &levelMeter() const;

public slots:
    void setName(const QString &name) override;
    void setChannelIndex(int index);
    void setControl(const TrackControl &control) override;

signals:
    void controlChanged(const TrackControl &control);

private:
    void onFaderMoved(double gain);
    void onFaderReleased(double gain);
    void onGainEdited(const QString &text);
    void onPeakChanged(double peak);

    void initUi();
    static QString gainValueToString(double gain);
    QHBoxLayout *buildFaderLevelMeterLayout();
    QVBoxLayout *buildChannelContentLayout();
    QStackedWidget *buildMuteSoloStack();
    QStackedWidget *buildIndexStack();

    Track *m_context = nullptr;
    bool m_isMasterChannel = false;
    bool m_notifyBarrier = false;

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