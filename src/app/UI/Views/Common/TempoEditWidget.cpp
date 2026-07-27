#include "TempoEditWidget.h"

#include <lite/GUI/Controls/SvsExpressionDoubleSpinBox.h>
#include <lite/GUI/Controls/TapTempoButton.h>

#include <QEvent>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <cmath>
#include <limits>
#include <numeric>

namespace {
    constexpr int kTapButtonHeight = 48;
    constexpr int kTapResetTimeoutMs = 3000;
    constexpr qsizetype kReadyTapIntervalCount = 16;
    constexpr qsizetype kMaxTapIntervalCount = 32;
    constexpr double kStableBpmHysteresis = 0.75;
}

TempoEditWidget::TempoEditWidget(QWidget *parent) : QWidget(parent) {
    m_spinTempo = new SVS::ExpressionDoubleSpinBox;
    m_spinTempo->setObjectName("spinTempo");
    m_spinTempo->setDecimals(3);
    m_spinTempo->setRange(0.001, std::numeric_limits<double>::max());
    m_spinTempo->setSingleStep(1.0);

    m_btnTapTempo = new TapTempoButton;
    m_btnTapTempo->setObjectName("btnTapTempo");
    m_btnTapTempo->setText(tr("Tap Tempo"));
    m_btnTapTempo->setFixedHeight(kTapButtonHeight);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(m_spinTempo);
    layout->addWidget(m_btnTapTempo);

    connect(m_spinTempo, &SVS::ExpressionDoubleSpinBox::valueChanged, this,
            &TempoEditWidget::tempoChanged);
    connect(m_btnTapTempo, &Button::pressed, this, &TempoEditWidget::recordTap);

    m_tapResetTimer.setSingleShot(true);
    m_tapResetTimer.setInterval(kTapResetTimeoutMs);
    connect(&m_tapResetTimer, &QTimer::timeout, this, &TempoEditWidget::expireTapTempo);
}

void TempoEditWidget::setTempo(const double tempo) {
    const QSignalBlocker blocker(m_spinTempo);
    m_spinTempo->setValue(tempo);
}

double TempoEditWidget::tempo() const {
    return m_spinTempo->value();
}

void TempoEditWidget::resetTapTempo() {
    expireTapTempo();
    m_hasDisplayedTapBpm = false;
    m_btnTapTempo->setProgressImmediately(0.0);
    m_btnTapTempo->setStable(false);
    m_btnTapTempo->setText(tr("Tap Tempo"));
}

void TempoEditWidget::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        m_btnTapTempo->setText(m_tapTimer.isValid() ? tr("Keep Tapping") : tr("Tap Tempo"));
}

void TempoEditWidget::recordTap() {
    if (!m_tapTimer.isValid()) {
        m_tapIntervals.clear();
        m_hasDisplayedTapBpm = false;
        m_tapTimer.start();
        m_btnTapTempo->setProgress(0.0);
        m_btnTapTempo->setStable(false);
        m_btnTapTempo->setText(tr("Keep Tapping"));
        m_tapResetTimer.start();
        return;
    }

    const qint64 interval = m_tapTimer.restart();
    if (interval >= kTapResetTimeoutMs) {
        m_tapIntervals.clear();
        m_hasDisplayedTapBpm = false;
        m_btnTapTempo->setProgress(0.0);
        m_btnTapTempo->setStable(false);
        m_btnTapTempo->setText(tr("Keep Tapping"));
        m_tapResetTimer.start();
        return;
    }
    if (interval <= 0) {
        m_tapResetTimer.start();
        return;
    }

    m_tapIntervals.append(interval);
    while (m_tapIntervals.size() > kMaxTapIntervalCount)
        m_tapIntervals.removeFirst();

    const qint64 totalInterval =
        std::accumulate(m_tapIntervals.cbegin(), m_tapIntervals.cend(), qint64(0));
    const double averageInterval = static_cast<double>(totalInterval) / m_tapIntervals.size();
    const double bpm = 60000.0 / averageInterval;
    if (!m_hasDisplayedTapBpm || m_tapIntervals.size() < kReadyTapIntervalCount ||
        std::abs(bpm - m_displayedTapBpm) > kStableBpmHysteresis) {
        m_displayedTapBpm = qRound(bpm);
        m_hasDisplayedTapBpm = true;
    }

    m_btnTapTempo->setText(QStringLiteral("%1 BPM").arg(m_displayedTapBpm));
    m_spinTempo->setValue(m_displayedTapBpm);
    const auto readyIntervalCount = qMin(m_tapIntervals.size(), kReadyTapIntervalCount);
    m_btnTapTempo->setProgress(static_cast<double>(readyIntervalCount) / kReadyTapIntervalCount);
    m_btnTapTempo->setStable(m_tapIntervals.size() >= kReadyTapIntervalCount);
    m_tapResetTimer.start();
}

void TempoEditWidget::expireTapTempo() {
    m_tapResetTimer.stop();
    m_tapTimer.invalidate();
    m_tapIntervals.clear();
}
