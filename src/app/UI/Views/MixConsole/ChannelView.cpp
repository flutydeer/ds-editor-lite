//
// Created by FlutyDeer on 2025/3/25.
//

#include "ChannelView.h"

#include "UI/Controls/Fader.h"
#include "UI/Controls/LevelMeter.h"

#include <QVBoxLayout>

ChannelView::ChannelView(QWidget *parent) : QWidget(parent) {
    m_fader = new Fader;
    m_fader->setStyleSheet("Fader { background: #21242B; border-radius: 4px; }");
    m_fader->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_fader->setFixedWidth(32);

    m_levelMeter = new LevelMeter;
    m_levelMeter->setStyleSheet("LevelMeter { background: #21242B; border-radius: 4px; }");
    m_levelMeter->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_levelMeter->setFixedWidth(32);

    auto faderLevelMeterLayout = new QHBoxLayout;
    faderLevelMeterLayout->addWidget(m_fader);
    faderLevelMeterLayout->addWidget(m_levelMeter);
    faderLevelMeterLayout->setContentsMargins({});
    faderLevelMeterLayout->setSpacing(8);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addLayout(faderLevelMeterLayout);
    setLayout(mainLayout);

    resize(97, 400);
    setWindowTitle("Master Channel");

    connect(m_levelMeter, &LevelMeter::peakValueChanged, this, [=](double value) {
        qDebug() << "Peak:"<< value << "dB";
    });
}

Fader *const &ChannelView::fader() const {
    return m_fader;
}

LevelMeter *const &ChannelView::levelMeter() const {
    return m_levelMeter;
}