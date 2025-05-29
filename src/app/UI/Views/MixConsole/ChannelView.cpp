//
// Created by FlutyDeer on 2025/3/25.
//

#include "ChannelView.h"

#include "UI/Controls/Fader.h"
#include "UI/Controls/LevelMeter.h"

#include <QVBoxLayout>

ChannelView::ChannelView(QWidget *parent) : QWidget(parent) {
    fader = new Fader;
    fader->setStyleSheet("Fader { background: #21242B; border-radius: 4px; }");
    fader->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    fader->setFixedWidth(32);

    levelMeter = new LevelMeter;
    levelMeter->setStyleSheet("LevelMeter { background: #21242B; border-radius: 4px; }");
    levelMeter->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    levelMeter->setFixedWidth(32);

    auto faderLevelMeterLayout = new QHBoxLayout;
    faderLevelMeterLayout->addWidget(fader);
    faderLevelMeterLayout->addWidget(levelMeter);
    faderLevelMeterLayout->setContentsMargins({});
    faderLevelMeterLayout->setSpacing(8);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addLayout(faderLevelMeterLayout);
    setLayout(mainLayout);

    resize(97, 400);
    setWindowTitle("Master Channel");
}
