#include <lite/GUI/Controls/Toast.h>

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QPropertyAnimation>
#include <QScreen>
#include <QVBoxLayout>

QPointer<QWidget> Toast::m_globalContext;

ToastWidget::ToastWidget(const QString &text, QWidget *parent) : QWidget(parent) {
    m_lbMessage = new QLabel(text);
    m_lbMessage->setObjectName("toastMessage");
    m_lbMessage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    m_lbMessage->setMinimumWidth(0);
    m_lbMessage->setWordWrap(false);

    m_cardLayout = new QVBoxLayout;
    m_cardLayout->addWidget(m_lbMessage);
    m_cardLayout->setContentsMargins({});

    const auto container = new QFrame;
    container->setObjectName("toastContainer");
    container->setLayout(m_cardLayout);
    container->setContentsMargins(12, 8, 12, 8);
    container->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    container->setMinimumWidth(0);

    m_shadowEffect = new QGraphicsDropShadowEffect(this);
    m_shadowEffect->setBlurRadius(36);
    m_shadowEffect->setColor(QColor(0, 0, 0, 32));
    m_shadowEffect->setOffset(0, 8);
    container->setGraphicsEffect(m_shadowEffect);

    const auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(container);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    setLayout(mainLayout);

    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setWindowOpacity(0);
}

QColor ToastWidget::shadowColor() const {
    return m_shadowEffect->color();
}

void ToastWidget::setShadowColor(const QColor &color) {
    m_shadowEffect->setColor(color);
}

void Toast::show(const QString &message) {
    instance()->m_queue.enqueue(message);
    if (!instance()->m_isShowingToast)
        instance()->showNextToast();
}

void Toast::afterSetAnimationEnabled(bool enabled) {
    Q_UNUSED(enabled)
    updateAnimationSettings();
}

void Toast::afterSetTimeScale(double scale) {
    Q_UNUSED(scale)
    updateAnimationSettings();
}

void Toast::hideToast() {
    if (!m_toastWidget)
        return;

    m_opacityAnimation.stop();
    m_opacityAnimation.setStartValue(m_toastWidget->windowOpacity());
    m_opacityAnimation.setEndValue(0);
    if (m_opacityAnimation.duration() == 0) {
        m_toastWidget->setWindowOpacity(0);
        destroyCurrentToast();
        return;
    }
    m_opacityAnimation.start();
    m_destroyWidgetTimer.start();
}

Toast::Toast(QObject *parent) : QObject(parent) {
    m_keepOnScreenTimer.setInterval(1500);
    m_keepOnScreenTimer.setSingleShot(true);
    connect(&m_keepOnScreenTimer, &QTimer::timeout, this, &Toast::hideToast);

    m_destroyWidgetTimer.setSingleShot(true);
    connect(&m_destroyWidgetTimer, &QTimer::timeout, this, &Toast::destroyCurrentToast);

    m_opacityAnimation.setPropertyName("windowOpacity");
    m_opacityAnimation.setEasingCurve(QEasingCurve::InOutCubic);

    m_posAnimation.setPropertyName("pos");
    m_posAnimation.setEasingCurve(QEasingCurve::OutQuart);

    initializeAnimation();
}

Toast::~Toast() {
    if (m_globalContext)
        m_globalContext->removeEventFilter(this);
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(Toast)

void Toast::setGlobalContext(QWidget *context) {
    m_globalContext = context;
}

bool Toast::eventFilter(QObject *watched, QEvent *event) {
    // 顶层 toast 不随主窗口自动移动，监听主窗口移动/隐藏事件保持相对位置
    if (watched == m_globalContext && m_toastWidget) {
        if (event->type() == QEvent::Move) {
            m_posAnimation.stop();
            m_toastWidget->move(targetPos());
        } else if (event->type() == QEvent::Hide || event->type() == QEvent::WindowStateChange) {
            if (!m_globalContext->isVisible())
                destroyCurrentToast();
        }
    }
    return QObject::eventFilter(watched, event);
}

QPoint Toast::targetPos() const {
    const auto contextGeometry = m_globalContext->geometry();
    const auto toastWidth = m_toastWidget ? m_toastWidget->geometry().width() : 0;
    return contextGeometry.center() - QPoint(toastWidth / 2, contextGeometry.height() / 2 - 96);
}

void Toast::showNextToast() {
    if (m_queue.count() <= 0)
        return;

    m_isShowingToast = true;
    // Parent to the global context (main window) so the toast inherits the
    // theme stylesheet cascade; window flags keep it a top-level window
    m_toastWidget = new ToastWidget(m_queue.dequeue(), m_globalContext);
    m_toastWidget->show();

    m_opacityAnimation.setTargetObject(m_toastWidget);
    m_posAnimation.setTargetObject(m_toastWidget);

    const auto toastWidth = m_toastWidget->geometry().width();
    if (m_globalContext) {
        m_globalContext->installEventFilter(this);
        const auto targetPos_ = targetPos();
        const auto startPos = QPoint(targetPos_.x(), targetPos_.y() - 32);
        m_posAnimation.setStartValue(startPos);
        m_posAnimation.setEndValue(targetPos_);
        if (m_posAnimation.duration() == 0)
            m_toastWidget->move(targetPos_);
        else
            m_posAnimation.start();
    } else
        m_toastWidget->move(QApplication::primaryScreen()->geometry().center() -
                            m_toastWidget->rect().center());

    m_opacityAnimation.stop();
    m_opacityAnimation.setStartValue(m_toastWidget->windowOpacity());
    m_opacityAnimation.setEndValue(1);
    if (m_opacityAnimation.duration() == 0)
        m_toastWidget->setWindowOpacity(1);
    else
        m_opacityAnimation.start();
    m_keepOnScreenTimer.start();
}

void Toast::oneToastShowFinished() {
    m_isShowingToast = false;
    showNextToast();
}

void Toast::destroyCurrentToast() {
    m_keepOnScreenTimer.stop();
    m_destroyWidgetTimer.stop();
    if (m_globalContext)
        m_globalContext->removeEventFilter(this);
    m_opacityAnimation.stop();
    m_posAnimation.stop();
    if (m_toastWidget) {
        m_toastWidget->hide();
        delete m_toastWidget;
        m_toastWidget = nullptr;
    }
    if (m_isShowingToast)
        oneToastShowFinished();
}

void Toast::updateAnimationSettings() {
    const auto opacityDuration = getEffectiveAnimationTime(animationDurationBase);
    const auto positionDuration = getEffectiveAnimationTime(animationDurationBase);

    const auto opacityRunning = m_opacityAnimation.state() == QAbstractAnimation::Running;
    const auto opacityEndValue = m_opacityAnimation.endValue().toDouble();
    const auto positionRunning = m_posAnimation.state() == QAbstractAnimation::Running;
    const auto positionEndValue = m_posAnimation.endValue().toPoint();

    m_opacityAnimation.stop();
    m_posAnimation.stop();
    m_opacityAnimation.setDuration(opacityDuration);
    m_posAnimation.setDuration(positionDuration);
    m_destroyWidgetTimer.setInterval(opacityDuration);

    if (m_destroyWidgetTimer.isActive() && opacityDuration == 0) {
        destroyCurrentToast();
        return;
    }

    if (positionRunning && m_toastWidget) {
        if (positionDuration == 0) {
            m_toastWidget->move(positionEndValue);
        } else {
            m_posAnimation.setStartValue(m_toastWidget->pos());
            m_posAnimation.setEndValue(positionEndValue);
            m_posAnimation.start();
        }
    }

    if (!opacityRunning || !m_toastWidget)
        return;

    if (opacityDuration == 0) {
        m_toastWidget->setWindowOpacity(opacityEndValue);
        if (qFuzzyIsNull(opacityEndValue))
            destroyCurrentToast();
        return;
    }

    m_opacityAnimation.setStartValue(m_toastWidget->windowOpacity());
    m_opacityAnimation.setEndValue(opacityEndValue);
    m_opacityAnimation.start();
    if (qFuzzyIsNull(opacityEndValue))
        m_destroyWidgetTimer.start();
}
