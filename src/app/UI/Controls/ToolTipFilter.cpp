//
// Created by fluty on 24-2-22.
//

#include <QApplication>
#include <QAbstractButton>
#include <QEvent>

#include "ToolTip.h"
#include "ToolTipFilter.h"

ToolTipFilter::ToolTipFilter(QWidget *parent, const int showDelay, const bool followCursor,
                             const bool animation)
    : QObject(parent) {
    m_parent = parent;
    parent->setAttribute(Qt::WA_Hover, true);
    m_showDelay = showDelay;
    m_followCursor = followCursor;

    m_tooltip = new ToolTip(m_parent->toolTip(), parent);
    m_tooltip->setAnimationEnabled(animation);
    if (const auto button = dynamic_cast<QAbstractButton *>(parent)) {
        const auto shortcut = button->shortcut();
        if (!shortcut.isEmpty())
            m_tooltip->setShortcutKey(shortcut.toString());
    }

    m_timer.setInterval(m_showDelay);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [&] {
        if (mouseInParent) {
            showToolTip();
        }
    });
}

bool ToolTipFilter::eventFilter(QObject *object, QEvent *event) {
    const auto type = event->type();
    if (type == QEvent::ToolTip)
        return true;
    if (type == QEvent::Hide || type == QEvent::Leave) {
        hideToolTip();
        mouseInParent = false;
    } else if (type == QEvent::Enter) {
        mouseInParent = true;
        m_timer.start();
    } else if (type == QEvent::MouseButtonPress) {
        hideToolTip();
    } else if (type == QEvent::HoverMove) {
        if (m_followCursor)
            m_tooltip->moveTo(QCursor::pos());
    }

    return QObject::eventFilter(object, event);
}

ToolTipFilter::~ToolTipFilter() {
    delete m_tooltip;
}

void ToolTipFilter::showToolTip() const {
    m_tooltip->setTitle(m_parent->toolTip());
    m_tooltip->showAt(QCursor::pos());
}

void ToolTipFilter::hideToolTip() const {
    m_tooltip->hideWithAnimation();
}

int ToolTipFilter::showDelay() const {
    return m_showDelay;
}

void ToolTipFilter::setShowDelay(const int delay) {
    m_showDelay = delay;
    m_timer.setInterval(m_showDelay);
}

bool ToolTipFilter::followCursor() const {
    return m_followCursor;
}

void ToolTipFilter::setFollowCursor(const bool on) {
    m_followCursor = on;
}

bool ToolTipFilter::animation() const {
    return m_tooltip->animationEnabled();
}

void ToolTipFilter::setAnimation(const bool on) {
    m_tooltip->setAnimationEnabled(on);
}

QString ToolTipFilter::title() const {
    return m_tooltip->title();
}

void ToolTipFilter::setTitle(const QString &text) const {
    m_tooltip->setTitle(text);
}

QString ToolTipFilter::shortcutKey() const {
    return m_tooltip->shortcutKey();
}

void ToolTipFilter::setShortcutKey(const QString &text) const {
    m_tooltip->setShortcutKey(text);
}

QList<QString> ToolTipFilter::message() const {
    return m_tooltip->message();
}

void ToolTipFilter::setMessage(const QList<QString> &text) const {
    m_tooltip->setMessage(text);
}

void ToolTipFilter::appendMessage(const QString &text) const {
    m_tooltip->appendMessage(text);
}

void ToolTipFilter::clearMessage() const {
    m_tooltip->clearMessage();
}
