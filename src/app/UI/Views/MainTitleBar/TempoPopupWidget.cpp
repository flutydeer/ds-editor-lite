#include "TempoPopupWidget.h"

#include "UI/Controls/Button.h"
#include "UI/Controls/SvsExpressionDoubleSpinBox.h"
#include "Utils/SystemUtils.h"

#ifdef Q_OS_WIN
#  include <Windows.h>
#  include <dwmapi.h>
#endif

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QScreen>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <limits>

namespace {
    constexpr int kEditorWidth = 120;
    constexpr int kTapResetTimeoutMs = 3000;
    constexpr qsizetype kMinimumTapCount = 4;
    constexpr qsizetype kMaxTapCount = 16;

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
    m_spinTempo->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_btnTapTempo = new Button;
    m_btnTapTempo->setObjectName("btnTapTempo");
    m_btnTapTempo->setText(tr("Tap Tempo"));
    m_btnTapTempo->setFixedHeight(28);

    auto *editorRow = new QHBoxLayout;
    editorRow->setContentsMargins(0, 0, 0, 0);
    editorRow->setSpacing(6);
    editorRow->addWidget(m_spinTempo);
    editorRow->addWidget(m_btnTapTempo);

    auto *surface = new QFrame;
    surface->setObjectName("tempoPopupSurface");
    surface->setAttribute(Qt::WA_StyledBackground);
    auto *surfaceLayout = new QVBoxLayout(surface);
    surfaceLayout->setContentsMargins(12, 12, 12, 12);
    surfaceLayout->setSpacing(8);
    surfaceLayout->addWidget(titleLabel);
    surfaceLayout->addLayout(editorRow);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->setSizeConstraint(QLayout::SetFixedSize);
    outerLayout->addWidget(surface);

    applyEditorGeometry();

    connect(m_spinTempo, &SVS::ExpressionDoubleSpinBox::valueChanged, this,
            &TempoPopupWidget::tempoSelected);
    connect(m_btnTapTempo, &Button::clicked, this, &TempoPopupWidget::recordTap);
}

void TempoPopupWidget::setTempo(double tempo) {
    const QSignalBlocker blocker(m_spinTempo);
    m_spinTempo->setValue(tempo);
}

void TempoPopupWidget::showAt(const QPoint &globalPos) {
    m_tapTimer.invalidate();
    m_tapTimes.clear();
    m_btnTapTempo->setText(tr("Tap Tempo"));

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
        m_tapTimes.clear();
        m_tapTimer.start();
        m_tapTimes.append(0);
        m_btnTapTempo->setText(tr("Keep Tapping"));
        return;
    }

    const qint64 tapTime = m_tapTimer.elapsed();
    if (tapTime - m_tapTimes.constLast() > kTapResetTimeoutMs) {
        m_tapTimer.restart();
        m_tapTimes = {0};
        m_btnTapTempo->setText(tr("Keep Tapping"));
        return;
    }

    m_tapTimes.append(tapTime);
    while (m_tapTimes.size() > kMaxTapCount)
        m_tapTimes.removeFirst();

    if (m_tapTimes.size() < kMinimumTapCount) {
        m_btnTapTempo->setText(tr("Keep Tapping"));
        return;
    }

    const qint64 duration = m_tapTimes.constLast() - m_tapTimes.constFirst();
    if (duration <= 0) {
        m_btnTapTempo->setText(tr("Keep Tapping"));
        return;
    }
    const double averageInterval = static_cast<double>(duration) / (m_tapTimes.size() - 1);
    const double bpm = 60000.0 / averageInterval;
    m_btnTapTempo->setText(QStringLiteral("%1 BPM").arg(qRound(bpm)));
}
