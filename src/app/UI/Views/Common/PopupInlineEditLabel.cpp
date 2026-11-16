#include "PopupInlineEditLabel.h"

#include <QApplication>
#include <QCursor>
#include <QFrame>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QStyle>
#include <QTimer>

PopupInlineEditLabel::PopupInlineEditLabel(QWidget *parent) : InlineEditLabel(parent) {
    m_clickTimer = new QTimer(this);
    m_clickTimer->setSingleShot(true);
    connect(m_clickTimer, &QTimer::timeout, this, &PopupInlineEditLabel::showPopup);
}

void PopupInlineEditLabel::setPopupWidget(QFrame *popup) {
    m_popup = popup;
    if (!m_popup)
        return;
    // The popup is a child so it shares this widget's lifetime, but it becomes
    // its own top-level window (Qt::Popup) once shown.
    m_popup->installEventFilter(this);
}

QFrame *PopupInlineEditLabel::popupWidget() const {
    return m_popup;
}

void PopupInlineEditLabel::preparePopup() {
}

void PopupInlineEditLabel::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton) {
        m_clickTimer->stop();
        if (isPopupVisible()) {
            m_hidingPopupFromCombo = true;
            hidePopup();
            m_hidingPopupFromCombo = false;
        } else {
            showPopup();
        }
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton) {
        InlineEditLabel::mousePressEvent(event);
        return;
    }

    if (isPopupVisible()) {
        m_hidingPopupFromCombo = true;
        hidePopup();
        m_hidingPopupFromCombo = false;
    } else {
        m_clickTimer->start(QApplication::doubleClickInterval());
    }
    event->accept();
}

void PopupInlineEditLabel::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        InlineEditLabel::mouseDoubleClickEvent(event);
        return;
    }

    m_clickTimer->stop();
    hidePopup();
    InlineEditLabel::mouseDoubleClickEvent(event);
}

bool PopupInlineEditLabel::eventFilter(QObject *watched, QEvent *event) {
    if (m_popup && watched == m_popup &&
        (event->type() == QEvent::Hide || event->type() == QEvent::Close)) {
        if (!m_hidingPopupFromCombo && (QGuiApplication::mouseButtons() & Qt::LeftButton) &&
            rect().contains(mapFromGlobal(QCursor::pos())))
            m_ignoreNextShow = true;
        setPopupVisible(false);
    }
    return InlineEditLabel::eventFilter(watched, event);
}

void PopupInlineEditLabel::showPopup() {
    if (m_ignoreNextShow) {
        m_ignoreNextShow = false;
        setPopupVisible(false);
        return;
    }
    if (!m_popup || m_popup->isVisible())
        return;

    emit popupAboutToShow();
    preparePopup();
    setPopupVisible(true);
    presentPopup(mapToGlobal(QPoint(0, height())));
}

void PopupInlineEditLabel::hidePopup() {
    if (m_popup && m_popup->isVisible())
        m_popup->close();
    setPopupVisible(false);
}

bool PopupInlineEditLabel::isPopupVisible() const {
    return m_popup && m_popup->isVisible();
}

void PopupInlineEditLabel::setPopupVisible(const bool visible) {
    if (property("popupVisible").toBool() == visible)
        return;
    setProperty("popupVisible", visible);
    style()->unpolish(this);
    style()->polish(this);
    update();
}
