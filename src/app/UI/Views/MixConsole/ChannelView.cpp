//
// Created by FlutyDeer on 2025/3/25.
//

#include "ChannelView.h"

#include "UI/Controls/Fader.h"
#include "UI/Controls/LevelMeter.h"
#include "UI/Controls/EditLabel.h"

#include <QVBoxLayout>
#include <QLabel>

ChannelView::ChannelView(QWidget *parent) : QWidget(parent) {
    m_fader = new Fader;
    m_fader->setObjectName("fader");
    m_fader->setStyleSheet("Fader { background: #21242B; border-radius: 4px; min-width: 32px}");
    m_fader->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);

    m_elGain = new EditLabel;
    m_elGain->setObjectName("elGain");
    m_elGain->setStyleSheet("EditLabel { min-width: 36px } EditLabel>QLabel { color: #8CD7D8DB }");
    m_elGain->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_elGain->label->setAlignment(Qt::AlignCenter);

    auto faderLayout = new QHBoxLayout;
    faderLayout->addStretch();
    faderLayout->addWidget(m_fader);
    faderLayout->addStretch();
    faderLayout->setContentsMargins({});

    auto faderGainLayout = new QVBoxLayout;
    faderGainLayout->addLayout(faderLayout);
    faderGainLayout->addWidget(m_elGain);
    faderGainLayout->setContentsMargins({});
    faderGainLayout->setSpacing(4);

    m_levelMeter = new LevelMeter;
    m_levelMeter->setObjectName("levelMeter");
    m_levelMeter->setStyleSheet(
        "LevelMeter { background: #21242B; border-radius: 4px; min-width: 32px }");
    m_levelMeter->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);

    m_lbPeakLevel = new QLabel;
    m_lbPeakLevel->setObjectName("lbPeakLevel");
    m_lbPeakLevel->setStyleSheet("QLabel { min-width: 36px; color: #8CD7D8DB }");
    m_lbPeakLevel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_lbPeakLevel->setAlignment(Qt::AlignCenter);

    auto levelMeterLayout = new QHBoxLayout;
    levelMeterLayout->addStretch();
    levelMeterLayout->addWidget(m_levelMeter);
    levelMeterLayout->addStretch();
    levelMeterLayout->setContentsMargins({});

    auto meterPeakLayout = new QVBoxLayout;
    meterPeakLayout->addLayout(levelMeterLayout);
    meterPeakLayout->addWidget(m_lbPeakLevel);
    meterPeakLayout->setSpacing(4);
    meterPeakLayout->setContentsMargins({});
    meterPeakLayout->setAlignment(Qt::AlignCenter);

    auto faderLevelMeterLayout = new QHBoxLayout;
    faderLevelMeterLayout->addLayout(faderGainLayout);
    faderLevelMeterLayout->addLayout(meterPeakLayout);
    faderLevelMeterLayout->setContentsMargins({});
    faderLevelMeterLayout->setSpacing(4);

    m_lbTitle = new QLabel;
    m_lbTitle->setObjectName("lbTitle");
    m_lbTitle->setStyleSheet("QLabel { min-width: 76px; color: #8CD7D8DB }");
    m_lbTitle->setAlignment(Qt::AlignCenter);

    auto channelContentLayout = new QVBoxLayout;
    // TODO: pan
    channelContentLayout->addLayout(faderLevelMeterLayout);
    // TODO: mute solo
    channelContentLayout->addWidget(m_lbTitle);
    channelContentLayout->setContentsMargins(10, 12, 10, 8);
    channelContentLayout->setSpacing(8);

    m_lbIndex = new QLabel;
    m_lbIndex->setObjectName("lbIndex");
    m_lbIndex->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_lbIndex->setStyleSheet("QLabel { color: #000000; background: #9BBAFF; min-height: 20px; border-bottom-left-radius: 4px; border-bottom-right-radius: 4px; }");
    m_lbIndex->setAlignment(Qt::AlignCenter);
    // m_lbIndex->setText("1");

    auto mainLayout = new QVBoxLayout;
    mainLayout->addLayout(channelContentLayout);
    mainLayout->addWidget(m_lbIndex);
    mainLayout->setContentsMargins(0, 0, 1, 1);
    mainLayout->setSpacing(0);
    setLayout(mainLayout);

    setStyleSheet("ChannelView {background:#272A33}");

    onFaderMoved(m_fader->value());
    connect(m_fader, &Fader::sliderMoved, this, &ChannelView::onFaderMoved);
    connect(m_fader, &Fader::valueChanged, this, &ChannelView::onFaderMoved);
    connect(m_elGain, &EditLabel::editCompleted, this, &ChannelView::onGainEdited);

    onPeakChanged(m_levelMeter->peakValue());
    connect(m_levelMeter, &LevelMeter::peakValueChanged, this, &ChannelView::onPeakChanged);
}

Fader *const &ChannelView::fader() const {
    return m_fader;
}

LevelMeter *const &ChannelView::levelMeter() const {
    return m_levelMeter;
}

void ChannelView::setChannelTitle(const QString &title) {
    m_lbTitle->setText(title);
}

void ChannelView::setChannelIndex(int index) {
    m_lbIndex->setText(QString::number(index));
}

void ChannelView::onFaderMoved(double gain) {
    m_elGain->setText(gainValueToString(gain));
}

void ChannelView::onGainEdited(const QString &text) {
    m_fader->setValue(text.toDouble());
}

void ChannelView::onPeakChanged(double peak) {
    m_lbPeakLevel->setText(gainValueToString(peak));
}

QString ChannelView::gainValueToString(double gain) {
    if (gain <= -54)
        return "-∞";
    auto absVal = QString::number(qAbs(gain), 'f', 1);
    QString sig = "";
    if (gain > 0) {
        sig = "+";
    } else if (gain < 0 && gain <= -0.1) {
        sig = "-";
    }
    return sig + absVal /* + "dB" */;
}