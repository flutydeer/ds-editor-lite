#include "SmoothScroller.h"

#include <QAbstractScrollArea>
#include <QEvent>
#include <QScrollBar>
#include <QWheelEvent>

SmoothScroller::SmoothScroller(QObject *parent) : QObject(parent) {
    m_hAnim.setEasingCurve(QEasingCurve::OutCubic);
    m_vAnim.setEasingCurve(QEasingCurve::OutCubic);
    m_hAnim.setPropertyName("value");
    m_vAnim.setPropertyName("value");
    connect(&m_hAnim, &QPropertyAnimation::finished, this, [this] { m_logicalH.reset(); });
    connect(&m_vAnim, &QPropertyAnimation::finished, this, [this] { m_logicalV.reset(); });
    updateAnimationDuration();
    initializeAnimation();
}

void SmoothScroller::attachTo(QAbstractScrollArea *area) {
    m_area = area;
    auto *viewport = area->viewport();
    viewport->installEventFilter(this);
    viewport->setMouseTracking(true);
    m_hAnim.setTargetObject(area->horizontalScrollBar());
    m_vAnim.setTargetObject(area->verticalScrollBar());
    // 端到端 install 后既有 OverlayScrollBar 不受影响：滚轮动画驱动的是
    // QAbstractItemView/QScrollArea 的 scrollbar value，OverlayScrollBar 的
    // valueChanged 联动是 1:1 镜像，天然跟随动画值变化。
}

bool SmoothScroller::eventFilter(QObject *watched, QEvent *event) {
    if (!m_area || watched != m_area->viewport() || event->type() != QEvent::Wheel) {
        return QObject::eventFilter(watched, event);
    }

    auto *wheelEvent = static_cast<QWheelEvent *>(event);

    // 记录步进到滑动窗口
    const auto &d = wheelEvent->angleDelta();
    const auto step = (qAbs(d.x()) == 0 && qAbs(d.y()) % 120 == 0) ||
                      (qAbs(d.x()) % 120 == 0 && qAbs(d.y()) == 0);
    if (m_stepWindow.size() == kWindowSize)
        m_stepWindow.removeFirst();
    m_stepWindow.append(step);

    // 窗口内出现任一非 120 倍数事件 → 触控板（直通），直到该事件滑出窗口。
    // 与编辑区同方向：默认鼠标动画（空窗口 all_of 恒真），检测到触控板特征才短暂锁直通。
    if (!std::all_of(m_stepWindow.cbegin(), m_stepWindow.cend(), [](bool v) { return v; })) {
        return QObject::eventFilter(watched, event); // 不消费，原生滚动
    }

    // 鼠标滚轮：动画驱动 scrollbar value
    const auto delta = wheelEvent->angleDelta();
    const bool horizontal = (qAbs(delta.x()) > qAbs(delta.y()));
    auto *bar = horizontal ? m_area->horizontalScrollBar() : m_area->verticalScrollBar();
    if (!bar || bar->maximum() <= bar->minimum())
        return QObject::eventFilter(watched, event); // 无滚动范围，原生处理

    const double perStep =
        (horizontal ? m_area->viewport()->width() : m_area->viewport()->height()) * 0.15;
    QPropertyAnimation *anim = horizontal ? &m_hAnim : &m_vAnim;
    std::optional<int> *logical = horizontal ? &m_logicalH : &m_logicalV;

    const auto activeEnd = (anim->state() == QAbstractAnimation::Running && logical->has_value())
                               ? **logical
                               : bar->value();
    auto endValue = static_cast<int>(activeEnd - perStep * delta.y() / 120.0);
    endValue = qBound(bar->minimum(), endValue, bar->maximum());

    if (bar->value() != endValue) {
        anim->stop();
        anim->setStartValue(bar->value());
        anim->setEndValue(endValue);
        *logical = endValue;
        anim->start();
    }
    return true; // 消费滚轮事件
}

void SmoothScroller::updateAnimationDuration() {
    const auto duration = getEffectiveAnimationTime(kBaseMs, AnimationGlobal::Full);
    auto heat = [this, duration](QPropertyAnimation &anim, std::optional<int> &logical,
                                 QScrollBar *bar) {
        const bool running = anim.state() == QAbstractAnimation::Running;
        const auto endValue = anim.endValue();
        anim.stop();
        anim.setDuration(duration);
        if (!running)
            return;
        if (duration == 0) {
            bar->setValue(endValue.toInt());
            logical.reset();
            return;
        }
        anim.setStartValue(anim.currentValue());
        anim.setEndValue(endValue);
        *logical = endValue.toInt();
        anim.start();
    };
    heat(m_hAnim, m_logicalH, m_area ? m_area->horizontalScrollBar() : nullptr);
    heat(m_vAnim, m_logicalV, m_area ? m_area->verticalScrollBar() : nullptr);
}

void SmoothScroller::afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) {
    Q_UNUSED(level)
    updateAnimationDuration();
}

void SmoothScroller::afterSetTimeScale(double scale) {
    Q_UNUSED(scale)
    updateAnimationDuration();
}
