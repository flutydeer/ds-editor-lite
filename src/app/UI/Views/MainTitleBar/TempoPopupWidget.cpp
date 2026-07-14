#include "TempoPopupWidget.h"

#include "UI/Controls/SvsExpressionDoubleSpinBox.h"
#include "UI/Controls/TapTempoButton.h"
#include "Utils/SystemUtils.h"

#ifdef Q_OS_WIN
#  include <Windows.h>
#  include <dwmapi.h>
#endif

#include <QApplication>
#include <QEvent>
#include <QLabel>
#include <QScreen>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <cmath>
#include <limits>
#include <numeric>

namespace {
    constexpr int kPopupWidth = 160;
    constexpr int kPopupMargin = 12;
    constexpr int kEditorWidth = kPopupWidth - kPopupMargin * 2;
    constexpr int kTapButtonHeight = 48;
    constexpr int kTapResetTimeoutMs = 3000;
    constexpr qsizetype kReadyTapIntervalCount = 16;
    constexpr qsizetype kMaxTapIntervalCount = 32;
    constexpr double kStableBpmHysteresis = 0.75;
}

TempoPopupWidget::TempoPopupWidget(QWidget *parent) : QFrame(parent) {
    setObjectName("tempoPopup");
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_WindowPropagation);
    setProperty("dwmBorder", false);

#ifdef Q_OS_WIN
    if (SystemUtils::isWindows11())
        setProperty("dwmBorder", true);
#endif

    auto *titleLabel = new QLabel(tr("Tempo"));
    titleLabel->setObjectName("popupTitle");
    titleLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_spinTempo = new SVS::ExpressionDoubleSpinBox;
    m_spinTempo->setObjectName("spinTempo");
    m_spinTempo->setDecimals(3);
    m_spinTempo->setRange(0.001, std::numeric_limits<double>::max());
    m_spinTempo->setSingleStep(1.0);

    m_btnTapTempo = new TapTempoButton;
    m_btnTapTempo->setObjectName("btnTapTempo");
    m_btnTapTempo->setText(tr("Tap Tempo"));
    m_btnTapTempo->setFixedHeight(kTapButtonHeight);

    auto *editorLayout = new QVBoxLayout;
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(6);
    editorLayout->addWidget(m_spinTempo);
    editorLayout->addWidget(m_btnTapTempo);

    auto *surface = new QFrame;
    surface->setObjectName("tempoPopupSurface");
    surface->setAttribute(Qt::WA_StyledBackground);
    auto *surfaceLayout = new QVBoxLayout(surface);
    surfaceLayout->setContentsMargins(kPopupMargin, kPopupMargin, kPopupMargin, kPopupMargin);
    surfaceLayout->setSpacing(8);
    surfaceLayout->addWidget(titleLabel);
    surfaceLayout->addLayout(editorLayout);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->setSizeConstraint(QLayout::SetFixedSize);
    outerLayout->addWidget(surface);

    applyEditorGeometry();

    connect(m_spinTempo, &SVS::ExpressionDoubleSpinBox::valueChanged, this,
            &TempoPopupWidget::tempoSelected);
    connect(m_btnTapTempo, &Button::pressed, this, &TempoPopupWidget::recordTap);

    m_tapResetTimer.setSingleShot(true);
    m_tapResetTimer.setInterval(kTapResetTimeoutMs);
    connect(&m_tapResetTimer, &QTimer::timeout, this, &TempoPopupWidget::expireTapTempo);
}

void TempoPopupWidget::setTempo(double tempo) {
    const QSignalBlocker blocker(m_spinTempo);
    m_spinTempo->setValue(tempo);
}

void TempoPopupWidget::showAt(const QPoint &globalPos) {
    resetTapTempo();

    ensurePolished();
    applyEditorGeometry();
    layout()->invalidate();
    layout()->activate();

    QPoint topLeft = globalPos;
    if (const auto screen = QApplication::screenAt(globalPos)) {
        const QRect available = screen->availableGeometry();
        const QRect popupRect(topLeft, sizeHint());
        if (popupRect.right() > available.right())
            topLeft.setX(available.right() - popupRect.width());
        if (popupRect.bottom() > available.bottom())
            topLeft.setY(globalPos.y() - popupRect.height());
        if (topLeft.x() < available.left())
            topLeft.setX(available.left());
        if (topLeft.y() < available.top())
            topLeft.setY(available.top());
    }

    move(topLeft);
    show();
    raise();
    applyWindowEffects();
}

void TempoPopupWidget::changeEvent(QEvent *event) {
    QFrame::changeEvent(event);
    if (event->type() == QEvent::StyleChange && m_spinTempo && m_btnTapTempo)
        applyEditorGeometry();
}

void TempoPopupWidget::applyEditorGeometry() {
    m_spinTempo->ensurePolished();
    m_btnTapTempo->ensurePolished();
    m_spinTempo->setFixedWidth(kEditorWidth);
    m_btnTapTempo->setFixedWidth(kEditorWidth);
}

void TempoPopupWidget::applyWindowEffects() {
#ifdef Q_OS_WIN
    if (!SystemUtils::isWindows11() || !winId())
        return;

    HWND hwnd = reinterpret_cast<HWND>(winId());
    constexpr int DWMWA_USE_IMMERSIVE_DARK_MODE_ = 20;
    constexpr int DWMWA_WINDOW_CORNER_PREFERENCE_ = 33;
    constexpr int DWMWA_NCRENDERING_POLICY_ = 2;
    DWMNCRENDERINGPOLICY ncrp = DWMNCRP_ENABLED;
    INT dwcp = 2;
    UINT dark = 1;
    DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY_, &ncrp, sizeof(ncrp));
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_, &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE_, &dwcp, sizeof(dwcp));
#endif
}

void TempoPopupWidget::recordTap() {
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
    const double averageInterval =
        static_cast<double>(totalInterval) / m_tapIntervals.size();
    const double bpm = 60000.0 / averageInterval;

    if (!m_hasDisplayedTapBpm || m_tapIntervals.size() < kReadyTapIntervalCount ||
        std::abs(bpm - m_displayedTapBpm) > kStableBpmHysteresis) {
        m_displayedTapBpm = qRound(bpm);
        m_hasDisplayedTapBpm = true;
    }

    m_btnTapTempo->setText(QStringLiteral("%1 BPM").arg(m_displayedTapBpm));
    const auto readyIntervalCount = qMin(m_tapIntervals.size(), kReadyTapIntervalCount);
    m_btnTapTempo->setProgress(static_cast<double>(readyIntervalCount) /
                               kReadyTapIntervalCount);
    m_btnTapTempo->setStable(m_tapIntervals.size() >= kReadyTapIntervalCount);
    m_tapResetTimer.start();
}

void TempoPopupWidget::resetTapTempo() {
    expireTapTempo();
    m_hasDisplayedTapBpm = false;
    m_btnTapTempo->setProgressImmediately(0.0);
    m_btnTapTempo->setStable(false);
    m_btnTapTempo->setText(tr("Tap Tempo"));
}

void TempoPopupWidget::expireTapTempo() {
    m_tapResetTimer.stop();
    m_tapTimer.invalidate();
    m_tapIntervals.clear();
}
