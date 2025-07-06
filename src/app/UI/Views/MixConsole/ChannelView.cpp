//
// Created by FlutyDeer on 2025/3/25.
//

#include "ChannelView.h"

#include "Model/AppModel/Track.h"
#include "Model/AppModel/TrackControl.h"
#include "UI/Controls/Fader.h"
#include "UI/Controls/LevelMeter.h"
#include "UI/Controls/EditLabel.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/PanSlider.h"

#include <QVBoxLayout>
#include <QLabel>

ChannelView::ChannelView(QWidget *parent) : QWidget(parent) {
    initUi();
}

ChannelView::ChannelView(Track &track, QWidget *parent): QWidget(parent), ITrack(track.id()),
                                                          m_context(&track) {
    initUi();
    ChannelView::setName(track.name());
    ChannelView::setControl(track.control());
}

Track &ChannelView::context() const {
    return *m_context;
}

void ChannelView::setIsMasterChannel(bool on) {
    m_isMasterChannel = on;
    if (on) {
        m_indexStack->setCurrentIndex(1); // Place holder
        m_muteSoloStack->setCurrentIndex(1);
    } else {
        m_indexStack->setCurrentIndex(0);
        m_muteSoloStack->setCurrentIndex(0);
    }
}

TrackControl ChannelView::control() const {
    TrackControl control;
    control.setGain(m_fader->value());
    control.setMute(m_btnMute->isChecked());
    control.setSolo(m_btnSolo->isChecked());
    return control;
}

QString ChannelView::name() const {
    return m_lbTitle->text();
}

QColor ChannelView::color() const {
    return {};
}

void ChannelView::setColor(const QColor &color) {
}

Fader *const &ChannelView::fader() const {
    return m_fader;
}

LevelMeter *const &ChannelView::levelMeter() const {
    return m_levelMeter;
}

void ChannelView::setName(const QString &name) {
    m_lbTitle->setText(name);
}

void ChannelView::setChannelIndex(int index) {
    m_lbIndex->setText(QString::number(index));
}

void ChannelView::setControl(const TrackControl &control) {
    m_notifyBarrier = true;
    m_fader->setValue(control.gain());
    m_btnMute->setChecked(control.mute());
    m_btnSolo->setChecked(control.solo());
    m_notifyBarrier = false;
}

void ChannelView::onFaderMoved(double gain) {
    m_elGain->setText(gainValueToString(gain));
}

void ChannelView::onFaderReleased(double gain) {
    m_elGain->setText(gainValueToString(gain));
    emit controlChanged(control());
}

void ChannelView::onGainEdited(const QString &text) {
    m_fader->setValue(text.toDouble());
}

void ChannelView::onPeakChanged(double peak) {
    m_lbPeakLevel->setText(gainValueToString(peak));
}

void ChannelView::initUi() {
    setAttribute(Qt::WA_StyledBackground);
    m_indexStack = buildIndexStack();

    auto mainLayout = new QVBoxLayout;
    mainLayout->addLayout(buildChannelContentLayout());
    mainLayout->addWidget(m_indexStack);
    mainLayout->setContentsMargins(0, 0, 1, 1);
    mainLayout->setSpacing(0);
    setLayout(mainLayout);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    onFaderMoved(m_fader->value());
    connect(m_fader, &Fader::sliderMoved, this, &ChannelView::onFaderMoved);
    connect(m_fader, &Fader::valueChanged, this, &ChannelView::onFaderReleased);
    connect(m_elGain, &EditLabel::editCompleted, this, &ChannelView::onGainEdited);

    onPeakChanged(m_levelMeter->peakValue());
    connect(m_levelMeter, &LevelMeter::peakValueChanged, this, &ChannelView::onPeakChanged);

    connect(m_btnMute, &QPushButton::clicked, this, [=] {
        emit controlChanged(control());
    });
    connect(m_btnSolo, &QPushButton::clicked, this, [=] {
        emit controlChanged(control());
    });
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

QHBoxLayout *ChannelView::buildFaderLevelMeterLayout() {
    m_fader = new Fader;
    m_fader->setObjectName("fader");
    m_fader->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);

    m_elGain = new EditLabel;
    m_elGain->setObjectName("elGain");
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
    faderGainLayout->setSpacing(2);

    m_levelMeter = new LevelMeter;
    m_levelMeter->setObjectName("levelMeter");
    m_levelMeter->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);

    m_lbPeakLevel = new QLabel;
    m_lbPeakLevel->setObjectName("lbPeakLevel");
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
    meterPeakLayout->setSpacing(2);
    meterPeakLayout->setContentsMargins({});
    meterPeakLayout->setAlignment(Qt::AlignCenter);

    auto faderLevelMeterLayout = new QHBoxLayout;
    faderLevelMeterLayout->addLayout(faderGainLayout);
    faderLevelMeterLayout->addLayout(meterPeakLayout);
    faderLevelMeterLayout->setContentsMargins({});
    faderLevelMeterLayout->setSpacing(4);
    return faderLevelMeterLayout;
}

QVBoxLayout *ChannelView::buildChannelContentLayout() {
    m_muteSoloStack = buildMuteSoloStack();

    m_lbTitle = new QLabel;
    m_lbTitle->setObjectName("lbTitle");
    m_lbTitle->setAlignment(Qt::AlignCenter);

    m_panSlider = new PanSlider;
    m_panSlider->setObjectName("panSlider");
    m_panSlider->setValue(1);

    auto channelContentLayout = new QVBoxLayout;
    // TODO: pan
    // channelContentLayout->addWidget(m_panSlider);
    channelContentLayout->addLayout(buildFaderLevelMeterLayout());
    channelContentLayout->addWidget(m_muteSoloStack);
    channelContentLayout->addWidget(m_lbTitle);
    channelContentLayout->setContentsMargins(10, 12, 10, 8);
    channelContentLayout->setSpacing(8);
    return channelContentLayout;
}

QStackedWidget *ChannelView::buildMuteSoloStack() {
    m_btnMute = new Button;
    m_btnMute->setObjectName("btnMute");
    m_btnMute->setCheckable(true);
    m_btnMute->setText("M");
    m_btnMute->setContentsMargins(0, 0, 0, 0);

    m_btnSolo = new Button;
    m_btnSolo->setObjectName("btnSolo");
    m_btnSolo->setCheckable(true);
    m_btnSolo->setText("S");
    m_btnSolo->setContentsMargins(0, 0, 0, 0);

    auto muteSoloLayout = new QHBoxLayout;
    muteSoloLayout->addStretch();
    muteSoloLayout->addWidget(m_btnMute);
    muteSoloLayout->addWidget(m_btnSolo);
    muteSoloLayout->addStretch();
    muteSoloLayout->setSpacing(8);
    muteSoloLayout->setContentsMargins({});

    auto muteSoloWidget = new QWidget;
    muteSoloWidget->setObjectName("muteSoloWidget");
    muteSoloWidget->setLayout(muteSoloLayout);
    muteSoloWidget->setContentsMargins({});

    auto placeHolderWidget = new QWidget;
    placeHolderWidget->setObjectName("muteSoloPlaceHolder");

    auto stack = new QStackedWidget;
    stack->setObjectName("muteSoloStack");
    stack->addWidget(muteSoloWidget);
    stack->addWidget(placeHolderWidget);
    stack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    return stack;
}

QStackedWidget *ChannelView::buildIndexStack() {
    m_lbIndex = new QLabel;
    m_lbIndex->setObjectName("lbIndex");
    m_lbIndex->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_lbIndex->setAlignment(Qt::AlignCenter);
    // m_lbIndex->setText("1");

    auto indexPlaceholder = new QWidget;
    indexPlaceholder->setObjectName("indexPlaceholder");
    indexPlaceholder->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto stack = new QStackedWidget;
    stack->setObjectName("indexStack");
    stack->addWidget(m_lbIndex);
    stack->addWidget(indexPlaceholder);
    stack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    return stack;
}