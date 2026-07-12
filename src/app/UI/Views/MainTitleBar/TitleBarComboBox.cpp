//
// Created by FlutyDeer on 2026/7/13.
//

#include "TitleBarComboBox.h"

#include "FilePopupWidget.h"
#include "UI/Utils/IconUtils.h"

#include <QMouseEvent>
#include <QCursor>
#include <QGuiApplication>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionToolButton>

namespace {

    constexpr int kComboHeight = 28;
    constexpr int kMinComboWidth = 84;
    constexpr int kMaxComboWidth = 480;
    constexpr int kTextLeftPadding = 12;
    constexpr int kTextArrowSpacing = 12;
    constexpr int kArrowAreaWidth = 28;
    constexpr int kArrowIconSize = 16;

}

TitleBarComboBox::TitleBarComboBox(QWidget *parent) : QToolButton(parent) {
    setObjectName("titleBarComboBox");
    setFixedHeight(kComboHeight);
    setMinimumWidth(kMinComboWidth);
    setMaximumWidth(kMaxComboWidth);
    setFocusPolicy(Qt::NoFocus);
    setToolButtonStyle(Qt::ToolButtonTextOnly);
    setAutoRaise(true);

    m_popup = new FilePopupWidget(this);
    m_popup->installEventFilter(this);
}

TitleBarComboBox::~TitleBarComboBox() = default;

void TitleBarComboBox::setTitle(const QString &title) {
    m_title = title;
    setText(title);
    setToolTip(title);
    const int titleWidth = fontMetrics().horizontalAdvance(title) + kTextLeftPadding +
                           kTextArrowSpacing + kArrowIconSize +
                           (kArrowAreaWidth - kArrowIconSize) / 2;
    setMinimumWidth(qBound(kMinComboWidth, titleWidth, kMaxComboWidth));
    updateGeometry();
    update();
}

FilePopupWidget *TitleBarComboBox::popupWidget() const {
    return m_popup;
}

QSize TitleBarComboBox::sizeHint() const {
    const auto text = m_title.isEmpty() ? QStringLiteral(" ") : m_title;
    const int width = fontMetrics().horizontalAdvance(text) + kTextLeftPadding +
                      kTextArrowSpacing + kArrowIconSize +
                      (kArrowAreaWidth - kArrowIconSize) / 2;
    return {qBound(kMinComboWidth, width, kMaxComboWidth), kComboHeight};
}

QSize TitleBarComboBox::minimumSizeHint() const {
    return {kMinComboWidth, kComboHeight};
}

void TitleBarComboBox::showPopup() {
    if (m_ignoreNextShow) {
        m_ignoreNextShow = false;
        setPopupVisible(false);
        return;
    }

    if (m_popup->isVisible())
        return;

    setPopupVisible(true);
    m_popup->showAt(mapToGlobal(QPoint(0, height())));
}

void TitleBarComboBox::hidePopup() {
    if (m_popup->isVisible())
        m_popup->close();
    setPopupVisible(false);
}

void TitleBarComboBox::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)

    QPainter painter(this);
    QStyleOptionToolButton option;
    option.initFrom(this);
    option.features = QStyleOptionToolButton::None;
    option.toolButtonStyle = Qt::ToolButtonTextOnly;
    style()->drawComplexControl(QStyle::CC_ToolButton, &option, &painter, this);

    const QColor textColor = option.palette.color(
        isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText);
    QRect textRect = option.rect;
    textRect.adjust(kTextLeftPadding, 0, -kArrowAreaWidth, 0);
    painter.setPen(textColor);
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, m_title);

    const QRect arrowRect(width() - kArrowAreaWidth, 0, kArrowAreaWidth, height());
    const QSize iconSize(16, 16);
    const QPoint iconPos(arrowRect.x() + (arrowRect.width() - iconSize.width()) / 2,
                         arrowRect.y() + (arrowRect.height() - iconSize.height()) / 2);
    const auto arrowIcon = IconUtils::createTintedSvgIcon(
        QStringLiteral(":/svg/icons/chevron_down_16_regular.svg"), iconSize, textColor,
        textColor);
    painter.drawPixmap(iconPos, arrowIcon.pixmap(iconSize, isEnabled() ? QIcon::Normal
                                                                         : QIcon::Disabled));
}

void TitleBarComboBox::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        QToolButton::mousePressEvent(event);
        return;
    }

    if (m_popup->isVisible()) {
        m_hidingPopupFromCombo = true;
        hidePopup();
        m_hidingPopupFromCombo = false;
    } else {
        showPopup();
    }
    event->accept();
}

bool TitleBarComboBox::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_popup && (event->type() == QEvent::Hide || event->type() == QEvent::Close)) {
        if (!m_hidingPopupFromCombo && (QGuiApplication::mouseButtons() & Qt::LeftButton) &&
            rect().contains(mapFromGlobal(QCursor::pos())))
            m_ignoreNextShow = true;
        setPopupVisible(false);
    }
    return QToolButton::eventFilter(watched, event);
}

void TitleBarComboBox::setPopupVisible(bool visible) {
    if (property("popupVisible").toBool() == visible)
        return;
    setProperty("popupVisible", visible);
    style()->unpolish(this);
    style()->polish(this);
    update();
}
