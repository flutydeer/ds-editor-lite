//
// Created by fluty on 24-2-22.
//

#include <QApplication>
#include <QPropertyAnimation>
#include <QEvent>
#include <QScreen>

#include "ToolTip.h"
#include "ToolTipFilter.h"

#include <QPushButton>

ToolTipFilter::ToolTipFilter(QWidget *parent, const int showDelay, const bool followCursor,
                             const bool animation)
    : QObject(parent) {
    m_parent = parent;
    parent->setAttribute(Qt::WA_Hover, true);
    m_showDelay = showDelay;
    m_followCursor = followCursor;
    m_animation = animation;

    m_tooltip = new ToolTip(m_parent->toolTip(), parent);
    if (const auto button = dynamic_cast<QAbstractButton *>(parent)) {
        const auto shortcut = button->shortcut();
        if (!shortcut.isEmpty())
            m_tooltip->setShortcutKey(shortcut.toString());
    }

    m_opacityAnimation = new QPropertyAnimation(m_tooltip, "windowOpacity");
    m_opacityAnimation->setDuration(150);

    m_timer.setInterval(m_showDelay);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [&] {
        if (mouseInParent) {
            adjustToolTipPos();
            showToolTip();
        }
    });
    connect(m_opacityAnimation, &QPropertyAnimation::finished, this, [this] {
        if (m_tooltip && m_tooltip->windowOpacity() == 0) {
            m_tooltip->hide();
        }
    });
}

bool ToolTipFilter::eventFilter(QObject *object, QEvent *event) {
    const auto type = event->type();
    if (type == QEvent::ToolTip)
        return true; // discard the original QToolTip event
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
            adjustToolTipPos();
    }

    return QObject::eventFilter(object, event);
}

ToolTipFilter::~ToolTipFilter() {
    delete m_tooltip;
    delete m_opacityAnimation;
}

void ToolTipFilter::adjustToolTipPos() const {
    if (m_tooltip == nullptr)
        return;

    auto getPos = [&] {
        const auto cursorPos = QCursor::pos();
        const auto screen = QApplication::screenAt(cursorPos);
        const auto screenRect = screen->availableGeometry();
        const auto toolTipRect = m_tooltip->rect();
        const auto left = screenRect.left();
        const auto top = screenRect.top();
        const auto width = screenRect.width() - toolTipRect.width();
        const auto height = screenRect.height() - toolTipRect.height();
        const auto availableRect = QRect(left, top, width, height);

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

void ToolTipFilter::showToolTip() const {
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

void ToolTipFilter::hideToolTip() const {
    if (m_animation) {
        m_opacityAnimation->stop();
        m_opacityAnimation->setStartValue(m_tooltip->windowOpacity());
        m_opacityAnimation->setEndValue(0);
        m_opacityAnimation->start();
    } else {
        m_tooltip->setWindowOpacity(0);
        m_tooltip->hide();
    }
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
    return m_animation;
}

void ToolTipFilter::setAnimation(const bool on) {
    m_animation = on;
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
