//
// Created by fluty on 2023/8/30.
//

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QScreen>
#include <QVBoxLayout>
#include <QLabel>

#include "ToolTip.h"

ToolTip::ToolTip(const QString &title, QWidget *parent) : QFrame(parent) {
    m_lbTitle = new QLabel(title);
    m_lbTitle->setObjectName("toolTipTitle");
    m_lbTitle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);

    m_lbShortcutKey = new QLabel();
    m_lbShortcutKey->setObjectName("toolTipShortcutKey");
    m_lbShortcutKey->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    m_lbShortcutKey->setVisible(false);

    const auto titleShortcutLayout = new QHBoxLayout;
    titleShortcutLayout->addWidget(m_lbTitle);
    titleShortcutLayout->addWidget(m_lbShortcutKey);
    titleShortcutLayout->setContentsMargins({});

    m_messageLayout = new QVBoxLayout;
    m_messageLayout->setContentsMargins({});
    m_messageLayout->setSpacing(0);

    m_cardLayout = new QVBoxLayout;
    m_cardLayout->addLayout(titleShortcutLayout);
    m_cardLayout->addLayout(m_messageLayout);
    m_cardLayout->setContentsMargins({});

    const auto container = new QFrame;
    container->setObjectName("toolTipContainer");
    container->setLayout(m_cardLayout);
    container->setContentsMargins(8, 4, 8, 4);
    container->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    m_shadowEffect = new QGraphicsDropShadowEffect(this);
    m_shadowEffect->setBlurRadius(24);
    m_shadowEffect->setColor(QColor(0, 0, 0, 32));
    m_shadowEffect->setOffset(0, 4);
    container->setGraphicsEffect(m_shadowEffect);

    const auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(container);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    setLayout(mainLayout);

    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setWindowOpacity(0);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);

    m_opacityAnimation = new QPropertyAnimation(this, "windowOpacity");
    m_opacityAnimation->setDuration(150);
    connect(m_opacityAnimation, &QPropertyAnimation::finished, this, [this] {
        if (windowOpacity() == 0) {
            hide();
            emit hideAnimationFinished();
        }
    });
}

ToolTip::~ToolTip() {
    delete m_opacityAnimation;
}

QColor ToolTip::shadowColor() const {
    return m_shadowEffect->color();
}

void ToolTip::setShadowColor(const QColor &color) {
    m_shadowEffect->setColor(color);
}

QString ToolTip::title() const {
    return m_title;
}

void ToolTip::setTitle(const QString &text) {
    m_title = text;
    m_lbTitle->setText(m_title);
}

QString ToolTip::shortcutKey() const {
    return m_shortcutKey;
}

void ToolTip::setShortcutKey(const QString &text) {
    m_lbShortcutKey->setVisible(true);
    m_shortcutKey = text;
    m_lbShortcutKey->setText(m_shortcutKey);
}

QList<QString> ToolTip::message() const {
    return m_message;
}

void ToolTip::setMessage(const QList<QString> &text) {
    m_message.clear();
    m_message.append(text);
    updateMessage();
}

void ToolTip::appendMessage(const QString &text) {
    m_message.append(text);
    updateMessage();
}

void ToolTip::clearMessage() {
    m_message.clear();
    updateMessage();
}

void ToolTip::setAnimationEnabled(bool on) {
    m_animationEnabled = on;
}

bool ToolTip::animationEnabled() const {
    return m_animationEnabled;
}

QPoint ToolTip::clampToScreen(const QPoint &screenPos) const {
    const auto screen = QApplication::screenAt(screenPos);
    if (!screen)
        return screenPos;

    const auto screenRect = screen->availableGeometry();
    const auto toolTipRect = rect();
    const auto left = screenRect.left();
    const auto top = screenRect.top();
    const auto width = screenRect.width() - toolTipRect.width();
    const auto height = screenRect.height() - toolTipRect.height();
    const auto availableRect = QRect(left, top, width, height);

    auto x = screenPos.x();
    auto y = screenPos.y();

    if (x < availableRect.left())
        x = availableRect.left();
    else if (x > availableRect.right())
        x = availableRect.right();

    if (y < availableRect.top())
        y = availableRect.top();
    else if (y > availableRect.bottom())
        y = availableRect.bottom();

    return QPoint(x, y);
}

void ToolTip::showAt(const QPoint &screenPos) {
    move(clampToScreen(screenPos));

    if (m_animationEnabled) {
        m_opacityAnimation->stop();
        m_opacityAnimation->setStartValue(windowOpacity());
        m_opacityAnimation->setEndValue(1);
        m_opacityAnimation->start();
    } else {
        setWindowOpacity(1);
    }

    show();
}

void ToolTip::moveTo(const QPoint &screenPos) {
    move(clampToScreen(screenPos));
}

void ToolTip::hideWithAnimation() {
    if (m_animationEnabled) {
        m_opacityAnimation->stop();
        m_opacityAnimation->setStartValue(windowOpacity());
        m_opacityAnimation->setEndValue(0);
        m_opacityAnimation->start();
    } else {
        setWindowOpacity(0);
        hide();
    }
}

void ToolTip::updateMessage() {
    QLayoutItem *child;
    while ((child = m_messageLayout->takeAt(0)) != nullptr) {
        child->widget()->setParent(nullptr);
        delete child;
    }

    for (const auto &message : m_message) {
        const auto label = new QLabel;
        label->setObjectName("toolTipMessage");
        label->setText(message);
        m_messageLayout->addWidget(label);
    }
}
