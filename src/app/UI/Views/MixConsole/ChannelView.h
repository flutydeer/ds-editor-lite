//
// Created by FlutyDeer on 2025/3/25.
//

#ifndef CHANNELVIEW_H
#define CHANNELVIEW_H

#include <QWidget>

class Button;
class QVBoxLayout;
class QHBoxLayout;
class QStackedWidget;
class QLabel;
class EditLabel;
class Fader;
class LevelMeter;

class ChannelView : public QWidget {
    Q_OBJECT

public:
    // enum ChannelType {
    //     Track,
    //     Master
    // };

    explicit ChannelView(QWidget *parent = nullptr);

    void setIsMasterChannel(bool on);

    // [[nodiscard]] ChannelType channelType() const;

    [[nodiscard]] Fader *const &fader() const;

    [[nodiscard]] LevelMeter *const &levelMeter() const;

public slots:
    void setChannelTitle(const QString &title);
    void setChannelIndex(int index);

private:
    void onFaderMoved(double gain);
    void onGainEdited(const QString &text);
    void onPeakChanged(double peak);

    static QString gainValueToString(double gain);
    QHBoxLayout *buildFaderLevelMeterLayout();
    QVBoxLayout *buildChannelContentLayout();
    QStackedWidget *buildMuteSoloStack();
    QStackedWidget *buildIndexStack();

    bool m_isMasterChannel = false;

    Fader *m_fader = nullptr;
    EditLabel *m_elGain = nullptr;

    LevelMeter *m_levelMeter = nullptr;
    QLabel *m_lbPeakLevel = nullptr;

    Button *m_btnMute;
    Button *m_btnSolo;

    QLabel *m_lbTitle = nullptr;
    QLabel *m_lbIndex  = nullptr;

    QStackedWidget *m_muteSoloStack = nullptr;
    QStackedWidget *m_indexStack = nullptr;
};

#endif // CHANNELVIEW_H