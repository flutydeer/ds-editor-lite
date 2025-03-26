//
// Created by FlutyDeer on 2025/3/25.
//

#include "ChannelView.h"

#include "UI/Controls/LevelMeter.h"

#include <QVBoxLayout>

ChannelView::ChannelView(QWidget *parent) : QWidget(parent) {
    levelMeter = new LevelMeter;
    levelMeter->setStyleSheet("LevelMeter { background: #21242B; border-radius: 4px; }");

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(levelMeter);
    setLayout(mainLayout);

    resize(97, 400);
}
