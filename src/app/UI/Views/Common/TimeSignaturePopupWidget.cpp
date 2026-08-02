#include "TimeSignaturePopupWidget.h"

#include "TimeSignatureEditWidget.h"
#include <lite/Support/SystemUtils.h>
#include <lite/GUI/Utils/WindowFrameUtils.h>

#include <QApplication>
#include <QLabel>
#include <QScreen>
#include <QVBoxLayout>

TimeSignaturePopupWidget::TimeSignaturePopupWidget(QWidget *parent) : QFrame(parent) {
    setObjectName("timeSignaturePopup");
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

    m_titleLabel = new QLabel(tr("Time Signature"));
    auto *titleLabel = m_titleLabel;
    titleLabel->setObjectName("popupTitle");

    m_editWidget = new TimeSignatureEditWidget;

    auto *surface = new QFrame;
    surface->setObjectName("timeSignaturePopupSurface");
    surface->setAttribute(Qt::WA_StyledBackground);
    auto *surfaceLayout = new QVBoxLayout(surface);
    surfaceLayout->setContentsMargins(12, 12, 12, 12);
    surfaceLayout->setSpacing(8);
    surfaceLayout->addWidget(titleLabel);
    surfaceLayout->addWidget(m_editWidget);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->setSizeConstraint(QLayout::SetFixedSize);
    outerLayout->addWidget(surface);

    connect(m_editWidget, &TimeSignatureEditWidget::timeSignatureEdited, this,
            &TimeSignaturePopupWidget::timeSignatureSelected);
}

void TimeSignaturePopupWidget::setTimeSignature(int numerator, int denominator) {
    m_editWidget->setTimeSignature(numerator, denominator);
}

void TimeSignaturePopupWidget::changeEvent(QEvent *event) {
    QFrame::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        m_titleLabel->setText(tr("Time Signature"));
}

void TimeSignaturePopupWidget::showAt(const QPoint &globalPos) {
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

void TimeSignaturePopupWidget::applyWindowEffects() {
    WindowFrameUtils::applyPopupEffects(this);
}
