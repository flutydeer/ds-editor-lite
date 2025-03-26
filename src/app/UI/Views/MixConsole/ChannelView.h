//
// Created by FlutyDeer on 2025/3/25.
//

#ifndef CHANNELVIEW_H
#define CHANNELVIEW_H

#include <QWidget>

class LevelMeter;

class ChannelView : public QWidget {
    Q_OBJECT

public:
    explicit ChannelView(QWidget *parent = nullptr);

    LevelMeter *levelMeter;
};

#endif // CHANNELVIEW_H