#include "LevelMeterViewModel.h"

#include <QTimer>
#include <QVariantAnimation>

#include "Utils/VolumeUtils.h"

LevelMeterViewModel::LevelMeterViewModel(QObject *parent) : QObject(parent) {
    m_leftChannel.peakHoldTimer = new QTimer(this);
    m_rightChannel.peakHoldTimer = new QTimer(this);
    m_leftChannel.peakHoldTimer->setSingleShot(true);
    m_rightChannel.peakHoldTimer->setSingleShot(true);

    m_leftChannel.decayAnimation = new QVariantAnimation(this);
    m_rightChannel.decayAnimation = new QVariantAnimation(this);

    connect(m_leftChannel.peakHoldTimer, &QTimer::timeout,
            [this] { startDecayAnimation(m_leftChannel); });

    connect(m_rightChannel.peakHoldTimer, &QTimer::timeout,
            [this] { startDecayAnimation(m_rightChannel); });

    auto setupAnimation = [&](QVariantAnimation *anim) {
        anim->setDuration(m_decayTime);
        anim->setEasingCurve(QEasingCurve::InOutCubic);
    };

    setupAnimation(m_leftChannel.decayAnimation);
    setupAnimation(m_rightChannel.decayAnimation);

    connect(m_leftChannel.decayAnimation, &QVariantAnimation::valueChanged,
            [this](const QVariant &value) { handleAnimationUpdate(value, m_leftChannel); });

    connect(m_rightChannel.decayAnimation, &QVariantAnimation::valueChanged,
            [this](const QVariant &value) { handleAnimationUpdate(value, m_rightChannel); });

    connect(m_leftChannel.decayAnimation, &QVariantAnimation::finished, [this] {
        if (m_leftChannel.isDecaying && !m_leftChannel.peakHoldTimer->isActive()) {
            m_leftChannel.displayedPeak = 0.0;
            m_leftChannel.isDecaying = false;
            emit peakChanged();
        }
    });

    connect(m_rightChannel.decayAnimation, &QVariantAnimation::finished, [this] {
        if (m_rightChannel.isDecaying && !m_rightChannel.peakHoldTimer->isActive()) {
            m_rightChannel.displayedPeak = 0.0;
            m_rightChannel.isDecaying = false;
            emit peakChanged();
        }
    });
}

void LevelMeterViewModel::setLevels(double dBL, double dBR) {
    m_leftChannel.currentLevel = VolumeUtils::dBToLinear(dBL);
    m_rightChannel.currentLevel = VolumeUtils::dBToLinear(dBR);
    auto clippedValueL = m_leftChannel.currentLevel;
    auto clippedValueR = m_rightChannel.currentLevel;

    if (m_leftChannel.currentLevel > 1) {
        m_leftChannel.clipped = true;
        clippedValueL = 1;
        emit clipStateChanged();
    }
    if (m_rightChannel.currentLevel > 1) {
        m_rightChannel.clipped = true;
        clippedValueR = 1;
        emit clipStateChanged();
    }

    updatePeakValue(m_leftChannel);
    updatePeakValue(m_rightChannel);

    Q_UNUSED(clippedValueL);
    Q_UNUSED(clippedValueR);

    emit levelChanged();
}

void LevelMeterViewModel::resetClip() {
    m_leftChannel.clipped = false;
    m_rightChannel.clipped = false;
    emit clipStateChanged();
}

double LevelMeterViewModel::peakValue() const {
    return VolumeUtils::linearTodB(getPeakValueForTextDisplaying());
}

double LevelMeterViewModel::levelL() const {
    return m_leftChannel.currentLevel;
}

double LevelMeterViewModel::levelR() const {
    return m_rightChannel.currentLevel;
}

double LevelMeterViewModel::displayedPeakL() const {
    return m_leftChannel.displayedPeak;
}

double LevelMeterViewModel::displayedPeakR() const {
    return m_rightChannel.displayedPeak;
}

bool LevelMeterViewModel::clippedL() const {
    return m_leftChannel.clipped;
}

bool LevelMeterViewModel::clippedR() const {
    return m_rightChannel.clipped;
}

void LevelMeterViewModel::startDecayAnimation(ChannelData &channel) {
    if (channel.displayedPeak <= 0.0)
        return;

    channel.isDecaying = true;
    channel.decayAnimation->stop();
    channel.decayAnimation->setStartValue(channel.displayedPeak);
    channel.decayAnimation->setEndValue(0.0);
    channel.decayAnimation->start();
}

void LevelMeterViewModel::updatePeakValue(ChannelData &channel) {
    if (channel.currentLevel > channel.displayedPeak) {
        cancelDecayAnimation(channel);

        channel.peak = channel.currentLevel;
        channel.displayedPeak = channel.peak;
        channel.peakHoldTimer->stop();
        channel.peakHoldTimer->start(m_peakHoldTime);

        notifyDisplayedPeakChange();
    }
}

void LevelMeterViewModel::cancelDecayAnimation(ChannelData &channel) {
    if (channel.isDecaying) {
        channel.decayAnimation->stop();
        channel.isDecaying = false;
    }
}

void LevelMeterViewModel::handleAnimationUpdate(const QVariant &value, ChannelData &channel) {
    channel.displayedPeak = value.toDouble();
    notifyDisplayedPeakChange();
    emit peakChanged();
}

void LevelMeterViewModel::notifyDisplayedPeakChange() {
    const auto peakValue = getPeakValueForTextDisplaying();
    if (!qFuzzyCompare(m_lastPeakValue, peakValue)) {
        emit peakValueChanged(VolumeUtils::linearTodB(peakValue));
        m_lastPeakValue = peakValue;
    }
}

double LevelMeterViewModel::getPeakValueForTextDisplaying() const {
    const auto peakL = qFuzzyCompare(m_leftChannel.displayedPeak, 1.0) &&
                               m_leftChannel.peak > m_leftChannel.displayedPeak
                           ? m_leftChannel.peak
                           : m_leftChannel.displayedPeak;
    const auto peakR = qFuzzyCompare(m_rightChannel.displayedPeak, 1.0) &&
                               m_rightChannel.peak > m_rightChannel.displayedPeak
                           ? m_rightChannel.peak
                           : m_rightChannel.displayedPeak;
    return std::max(peakL, peakR);
}
