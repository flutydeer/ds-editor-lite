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
    m_animationEnabled = animation;

    m_tooltip = new ToolTip(m_parent->toolTip(), parent);
    m_tooltip->setAnimationEnabled(animation);
    if (const auto button = dynamic_cast<QAbstractButton *>(parent)) {
        const auto shortcut = button->shortcut();
        if (!shortcut.isEmpty()) {
            m_shortcutKey = shortcut.toString();
            m_tooltip->setShortcutKey(shortcut.toString());
        }
    }
    connect(m_tooltip, &ToolTip::hideAnimationFinished, this, [this] {
        m_tooltip->deleteLater();
        m_tooltip = nullptr;
    });

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
        if (m_followCursor && m_tooltip)
            m_tooltip->moveTo(QCursor::pos());
    }

    return QObject::eventFilter(object, event);
}

ToolTipFilter::~ToolTipFilter() {
    delete m_tooltip;
}

void ToolTipFilter::showToolTip() {
    if (!m_tooltip) {
        m_tooltip = new ToolTip(m_parent->toolTip(), m_parent);
        m_tooltip->setAnimationEnabled(m_animationEnabled);
        if (!m_shortcutKey.isEmpty())
            m_tooltip->setShortcutKey(m_shortcutKey);
        m_tooltip->setMessage(m_message);
        connect(m_tooltip, &ToolTip::hideAnimationFinished, this, [this] {
            m_tooltip->deleteLater();
            m_tooltip = nullptr;
        });
    }
    m_tooltip->setTitle(m_parent->toolTip());
    m_tooltip->showAt(QCursor::pos());
}

void ToolTipFilter::hideToolTip() {
    if (m_tooltip)
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
    return m_animationEnabled;
}

void ToolTipFilter::setAnimation(const bool on) {
    m_animationEnabled = on;
    if (m_tooltip)
        m_tooltip->setAnimationEnabled(on);
}

QString ToolTipFilter::title() const {
    return m_parent->toolTip();
}

void ToolTipFilter::setTitle(const QString &text) const {
    m_parent->setToolTip(text);
    if (m_tooltip)
        m_tooltip->setTitle(text);
}

QString ToolTipFilter::shortcutKey() const {
    return m_shortcutKey;
}

void ToolTipFilter::setShortcutKey(const QString &text) {
    m_shortcutKey = text;
    if (m_tooltip)
        m_tooltip->setShortcutKey(text);
}

QList<QString> ToolTipFilter::message() const {
    return m_message;
}

void ToolTipFilter::setMessage(const QList<QString> &text) {
    m_message = text;
    if (m_tooltip)
        m_tooltip->setMessage(text);
}

void ToolTipFilter::appendMessage(const QString &text) {
    m_message.append(text);
    if (m_tooltip)
        m_tooltip->appendMessage(text);
}

void ToolTipFilter::clearMessage() {
    m_message.clear();
    if (m_tooltip)
        m_tooltip->clearMessage();
}
