//
// Created by fluty on 24-2-22.
//

#include <QApplication>
#include <QPropertyAnimation>
#include <QEvent>
#include <QScreen>

#include "ToolTip.h"
#include "ToolTipFilter.h"

ToolTipFilter::ToolTipFilter(QWidget *parent, int showDelay, bool followCursor, bool animation)
    : QObject(parent) {
    m_parent = parent;
    parent->setAttribute(Qt::WA_Hover, true);
    m_showDelay = showDelay;
    m_followCursor = followCursor;
    m_animation = animation;

    m_tooltip = new ToolTip(m_parent->toolTip(), parent);

    m_opacityAnimation = new QPropertyAnimation(m_tooltip, "windowOpacity");
    m_opacityAnimation->setDuration(150);

    m_timer.setInterval(m_showDelay);
    m_timer.setSingleShot(true);
    QObject::connect(&m_timer, &QTimer::timeout, this, [&]() {
        if (mouseInParent) {
            adjustToolTipPos();
            showToolTip();
        }
    });
}

bool ToolTipFilter::eventFilter(QObject *object, QEvent *event) {
    auto type = event->type();
    if (type == QEvent::ToolTip)
        return true; // discard the original QToolTip event
    else if (type == QEvent::Hide || type == QEvent::Leave) {
        hideToolTip();
        mouseInParent = false;
    } else if (type == QEvent::Enter) {
        mouseInParent = true;
        m_timer.start();
    } else if (type == QEvent::MouseButtonPress) {
        hideToolTip();
    } else if (type == QEvent::HoverMove) {
        if (m_followCursor)
            adjustToolTipPos();
    }

    return QObject::eventFilter(object, event);
}

ToolTipFilter::~ToolTipFilter() {
    delete m_tooltip;
    delete m_opacityAnimation;
}

void ToolTipFilter::adjustToolTipPos() {
    if (m_tooltip == nullptr)
        return;

    auto getPos = [&]() {
        auto cursorPos = QCursor::pos();
        auto screen = QApplication::screenAt(cursorPos);
        auto screenRect = screen->availableGeometry();
        auto toolTipRect = m_tooltip->rect();
        auto left = screenRect.left();
        auto top = screenRect.top();
        auto width = screenRect.width() - toolTipRect.width();
        auto height = screenRect.height() - toolTipRect.height();
        auto availableRect = QRect(left, top, width, height);

        auto x = cursorPos.x();
        auto y = cursorPos.y();
        //        if (!availableRect.contains(cursorPos)) {
        //            qDebug() << "Out of available area";
        //        }

        if (x < availableRect.left())
            x = availableRect.left();
        else if (x > availableRect.right())
            x = availableRect.right();

        if (y < availableRect.top())
            y = availableRect.top();
        else if (y > availableRect.bottom())
            y = availableRect.bottom();

        return QPoint(x, y);
    };

    m_tooltip->move(getPos());
}

void ToolTipFilter::showToolTip() {
    if (m_animation) {
        m_opacityAnimation->stop();
        m_opacityAnimation->setStartValue(m_tooltip->windowOpacity());
        m_opacityAnimation->setEndValue(1);
        m_opacityAnimation->start();
    } else {
        m_tooltip->setWindowOpacity(1);
    }

    m_tooltip->setTitle(m_parent->toolTip());
    m_tooltip->show();
}

void ToolTipFilter::hideToolTip() {
    if (m_animation) {
        m_opacityAnimation->stop();
        m_opacityAnimation->setStartValue(m_tooltip->windowOpacity());
        m_opacityAnimation->setEndValue(0);
        m_opacityAnimation->start();
    } else {
        m_tooltip->setWindowOpacity(0);
    }
}

int ToolTipFilter::showDelay() const {
    return m_showDelay;
}

void ToolTipFilter::setShowDelay(int delay) {
    m_showDelay = delay;
}

bool ToolTipFilter::followCursor() const {
    return m_followCursor;
}

void ToolTipFilter::setFollowCursor(bool on) {
    m_followCursor = on;
}

bool ToolTipFilter::animation() const {
    return m_animation;
}

void ToolTipFilter::setAnimation(bool on) {
    m_animation = on;
}

QString ToolTipFilter::title() const {
    return m_tooltip->title();
}

void ToolTipFilter::setTitle(const QString &text) {
    m_tooltip->setTitle(text);
}

QString ToolTipFilter::shortcutKey() const {
    return m_tooltip->shortcutKey();
}

void ToolTipFilter::setShortcutKey(const QString &text) {
    m_tooltip->setShortcutKey(text);
}

QList<QString> ToolTipFilter::message() const {
    return m_tooltip->message();
}

void ToolTipFilter::setMessage(const QList<QString> &text) {
    m_tooltip->setMessage(text);
}

void ToolTipFilter::appendMessage(const QString &text) {
    m_tooltip->appendMessage(text);
}

void ToolTipFilter::clearMessage() {
    m_tooltip->clearMessage();
}
