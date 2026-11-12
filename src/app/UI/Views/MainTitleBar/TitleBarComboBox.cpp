#include "TitleBarComboBox.h"

#include "FilePopupWidget.h"
#include <lite/GUI/Utils/IconUtils.h>

#include <QMouseEvent>
#include <QCursor>
#include <QGuiApplication>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionToolButton>

namespace {

    constexpr int kComboHeight = 28;
    // The selector shows the project identity, but it is only one section of
    // the title bar. Its preferred width follows the title inside this range,
    // MainTitleBar caps it so it gives way before the menu bar has to collapse
    // any menu, and the text elides instead of running under the arrow.
    constexpr int kMinComboWidth = 80;
    constexpr int kMaxComboWidth = 360;
    constexpr int kTextLeftPadding = 12;
    constexpr int kTextArrowSpacing = 12;
    constexpr int kArrowAreaWidth = 28;
    constexpr int kArrowIconSize = 16;

    // Left padding plus the arrow area on the right, measured the same way
    // paintEvent() lays the text out.
    int preferredWidthForText(const QFontMetrics &metrics, const QString &text) {
        const int textWidth = metrics.horizontalAdvance(text) + kTextLeftPadding +
                              kTextArrowSpacing + kArrowIconSize +
                              (kArrowAreaWidth - kArrowIconSize) / 2;
        return qBound(kMinComboWidth, textWidth, kMaxComboWidth);
    }

}

TitleBarComboBox::TitleBarComboBox(QWidget *parent) : QToolButton(parent) {
    setObjectName("titleBarComboBox");
    setFixedHeight(kComboHeight);
    setMinimumWidth(kMinComboWidth);
    setMaximumWidth(kMaxComboWidth);
    // A tool button is Fixed by default, which pins it to sizeHint. Preferred
    // lets the title bar layout shrink the selector when the window is narrow,
    // with minimumSizeHint as the floor.
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setFocusPolicy(Qt::NoFocus);
    setToolButtonStyle(Qt::ToolButtonTextOnly);
    setAutoRaise(true);

    m_popup = new FilePopupWidget(this);
    m_popup->installEventFilter(this);
}

TitleBarComboBox::~TitleBarComboBox() = default;

void TitleBarComboBox::setTitle(const QString &title) {
    if (m_title == title)
        return;
    m_title = title;
    setText(title);
    // The visible text is elided once the bar squeezes the selector, so the
    // tooltip is the place that always carries the complete project name.
    setToolTip(title);
    updateGeometry();
    update();
}

FilePopupWidget *TitleBarComboBox::popupWidget() const {
    return m_popup;
}

QString TitleBarComboBox::visibleTitle() const {
    const int textWidth = width() - kTextLeftPadding - kArrowAreaWidth;
    if (textWidth <= 0)
        return {};
    return fontMetrics().elidedText(m_title, Qt::ElideRight, textWidth);
}

QSize TitleBarComboBox::sizeHint() const {
    const auto text = m_title.isEmpty() ? QStringLiteral(" ") : m_title;
    return {preferredWidthForText(fontMetrics(), text), kComboHeight};
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
    // The selector has a fixed maximum and can be squeezed, so a long title
    // must never paint into the arrow area. Elide it and let the tooltip carry
    // the full name.
    if (textRect.width() > 0)
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, visibleTitle());

    const QRect arrowRect(width() - kArrowAreaWidth, 0, kArrowAreaWidth, height());
    const QSize iconSize(16, 16);
    const QPoint iconPos(arrowRect.x() + (arrowRect.width() - iconSize.width()) / 2,
                         arrowRect.y() + (arrowRect.height() - iconSize.height()) / 2);
    const auto arrowIcon = IconUtils::createTintedSvgIcon(
        QStringLiteral(":/svg/icons/chevron_down_16_regular.svg"), iconSize, textColor, textColor);
    painter.drawPixmap(iconPos,
                       arrowIcon.pixmap(iconSize, isEnabled() ? QIcon::Normal : QIcon::Disabled));
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
