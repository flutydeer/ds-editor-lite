//
// Created by fluty on 2024/7/14.
//

#include "Toast.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QPropertyAnimation>
#include <QScreen>
#include <QVBoxLayout>

QWidget *Toast::m_globalContext = nullptr;

ToastWidget::ToastWidget(const QString &text, QWidget *parent) : QWidget(parent) {
    m_lbMessage = new QLabel(text);
    m_lbMessage->setStyleSheet("color: #F0F0F0; font-size: 10.5pt");
    m_lbMessage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    m_lbMessage->setMinimumWidth(0);
    m_lbMessage->setWordWrap(false);

    m_cardLayout = new QVBoxLayout;
    m_cardLayout->addWidget(m_lbMessage);
    m_cardLayout->setContentsMargins({});

    auto container = new QFrame;
    container->setObjectName("container");
    container->setLayout(m_cardLayout);
    container->setContentsMargins(12, 8, 12, 8);
    container->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    container->setMinimumWidth(0);
    container->setStyleSheet("QFrame#container {"
                             "background: #80202122; "
                             "border: 1px solid #80606060; "
                             "border-radius: 6px; "
                             "font-size: 10pt }");

    auto shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setBlurRadius(36);
    shadowEffect->setColor(QColor(0, 0, 0, 32));
    shadowEffect->setOffset(0, 8);
    container->setGraphicsEffect(shadowEffect);

    auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(container);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    setLayout(mainLayout);

    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setWindowOpacity(0);
}

void Toast::show(const QString &message) {
    instance()->m_queue.append(message);
    if (!instance()->m_isShowingToast)
        instance()->showNextToast();
}
void Toast::afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) {
}
void Toast::afterSetTimeScale(double scale) {
    m_opacityAnimation.setDuration(getScaledAnimationTime(animationDurationBase));
    m_posAnimation.setDuration(getScaledAnimationTime(animationDurationBase));
    m_destroyWidgetTimer.setInterval(getScaledAnimationTime(animationDurationBase));
}
void Toast::hideToast() {
    m_opacityAnimation.stop();
    m_opacityAnimation.setStartValue(m_toastWidget->windowOpacity());
    m_opacityAnimation.setEndValue(0);
    m_opacityAnimation.start();
    m_destroyWidgetTimer.start();
}
Toast::Toast(QObject *parent) : QObject(parent) {
    m_keepOnScreenTimer.setInterval(1500);
    m_keepOnScreenTimer.setSingleShot(true);
    connect(&m_keepOnScreenTimer, &QTimer::timeout, this, &Toast::hideToast);

    m_destroyWidgetTimer.setSingleShot(true);
    connect(&m_destroyWidgetTimer, &QTimer::timeout, this, [=] {
        m_toastWidget->hide();
        delete m_toastWidget;
        oneToastShowFinished();
    });

    auto duration = animationLevel() == AnimationGlobal::None
                        ? 0
                        : getScaledAnimationTime(animationDurationBase);

    m_opacityAnimation.setPropertyName("windowOpacity");
    m_opacityAnimation.setEasingCurve(QEasingCurve::InOutCubic);
    m_opacityAnimation.setDuration(duration);

    m_posAnimation.setPropertyName("pos");
    m_posAnimation.setEasingCurve(QEasingCurve::OutQuart);
    m_posAnimation.setDuration(duration);

    initializeAnimation();
}
void Toast::setGlobalContext(QWidget *context) {
    m_globalContext = context;
}
void Toast::showNextToast() {
    if (m_queue.count() <= 0)
        return;

    m_isShowingToast = true;
    m_toastWidget = new ToastWidget(m_queue.first());
    m_toastWidget->show();

    m_opacityAnimation.setTargetObject(m_toastWidget);
    m_posAnimation.setTargetObject(m_toastWidget);

    auto toastWidth = m_toastWidget->geometry().width();
    // qDebug() << toastWidth;
    if (m_globalContext) {
        auto targetPos = m_globalContext->geometry().center() -
                         QPoint(toastWidth / 2, m_globalContext->geometry().height() / 2 - 96);
        auto startPos = QPoint(targetPos.x(), targetPos.y() - 32);
        m_posAnimation.setStartValue(startPos);
        m_posAnimation.setEndValue(targetPos);
        m_posAnimation.start();
    } else
        m_toastWidget->move(QApplication::primaryScreen()->geometry().center() -
                            m_toastWidget->rect().center());

    m_opacityAnimation.stop();
    m_opacityAnimation.setStartValue(m_toastWidget->windowOpacity());
    m_opacityAnimation.setEndValue(1);
    m_opacityAnimation.start();
    m_keepOnScreenTimer.start();
}
void Toast::oneToastShowFinished() {
    m_queue.removeFirst();
    m_isShowingToast = false;
    showNextToast();
}
