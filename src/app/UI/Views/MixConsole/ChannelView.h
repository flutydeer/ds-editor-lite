//
// Created by FlutyDeer on 2025/3/25.
//

#ifndef CHANNELVIEW_H
#define CHANNELVIEW_H

#include <QWidget>

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

    bool m_isMasterChannel = false;

    Fader *m_fader;
    EditLabel *m_elGain;

    LevelMeter *m_levelMeter;
    QLabel *m_lbPeakLevel;

    QLabel *m_lbTitle;
    QLabel *m_lbIndex;
};

#endif // CHANNELVIEW_H