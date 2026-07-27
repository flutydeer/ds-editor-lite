#include "TempoPopupWidget.h"

#include "UI/Views/Common/TempoEditWidget.h"
#include <lite/Support/SystemUtils.h>
#include <lite/GUI/Utils/WindowFrameUtils.h>

#include <QApplication>
#include <QEvent>
#include <QLabel>
#include <QScreen>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {
    constexpr int kPopupWidth = 160;
    constexpr int kPopupMargin = 12;
    constexpr int kEditorWidth = kPopupWidth - kPopupMargin * 2;
}

TempoPopupWidget::TempoPopupWidget(QWidget *parent) : QFrame(parent) {
    setObjectName("tempoPopup");
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    // Keep this popup opaque: WA_TranslucentBackground plus DWM frame effects on the same
    // window can freeze the compositor system-wide. Corner rounding comes from DWM on
    // Windows 11 and stays square elsewhere.
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_WindowPropagation);
    setProperty("dwmBorder", false);

#ifdef Q_OS_WIN
    if (SystemUtils::isWindows11())
        setProperty("dwmBorder", true);
#endif

    m_titleLabel = new QLabel(tr("Tempo"));
    auto *titleLabel = m_titleLabel;
    titleLabel->setObjectName("popupTitle");
    titleLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_editWidget = new TempoEditWidget;
    m_editWidget->setFixedWidth(kEditorWidth);

    auto *surface = new QFrame;
    surface->setObjectName("tempoPopupSurface");
    surface->setAttribute(Qt::WA_StyledBackground);
    auto *surfaceLayout = new QVBoxLayout(surface);
    surfaceLayout->setContentsMargins(kPopupMargin, kPopupMargin, kPopupMargin, kPopupMargin);
    surfaceLayout->setSpacing(8);
    surfaceLayout->addWidget(titleLabel);
    surfaceLayout->addWidget(m_editWidget);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->setSizeConstraint(QLayout::SetFixedSize);
    outerLayout->addWidget(surface);

    connect(m_editWidget, &TempoEditWidget::tempoChanged, this, &TempoPopupWidget::tempoSelected);
}

void TempoPopupWidget::setTempo(double tempo) {
    m_editWidget->setTempo(tempo);
}

void TempoPopupWidget::showAt(const QPoint &globalPos) {
    m_editWidget->resetTapTempo();

    ensurePolished();
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
    if (event->type() == QEvent::LanguageChange)
        m_titleLabel->setText(tr("Tempo"));
}

void TempoPopupWidget::applyWindowEffects() {
    WindowFrameUtils::applyPopupEffects(this);
}
