#ifndef DSEDITORLITE_SMOOTHSCROLLER_H
#define DSEDITORLITE_SMOOTHSCROLLER_H

#include <lite/GUI/Animation/IAnimatable.h>

#include <QObject>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QVector>

#include <optional>

class QAbstractScrollArea;
class QScrollBar;
class QWheelEvent;

/// 平滑滚动器：拦截托管滚动区视口的滚轮事件，用 OutCubic 动画驱动 scrollbar 值。
/// 鼠标滚轮（angleDelta 为 120 整数倍）→ 动画；触控板（非整倍数/像素平滑）→ 直通。
class SmoothScroller : public QObject, public IAnimatable {
    Q_OBJECT

public:
    explicit SmoothScroller(QObject *parent = nullptr);
    /// 绑定滚动区（安装到其 viewport 的事件过滤器）。
    void attachTo(QAbstractScrollArea *area);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) override;
    void afterSetTimeScale(double scale) override;

private:
    void updateAnimationDuration();

    QAbstractScrollArea *m_area = nullptr;
    QPropertyAnimation m_hAnim;
    QPropertyAnimation m_vAnim;
    // 动画进行中的逻辑目标（下一轮滚轮叠加于此，而非打断点），溢出后清除。
    std::optional<int> m_logicalH;
    std::optional<int> m_logicalV;
    // 判定滑动窗口：最近 kWindowSize 个事件出现任一非 120 倍数即转触控板直通，
    // 直到该事件滑出窗口（期间默认鼠标动画，与编辑区方向一致）。
    QVector<bool> m_stepWindow;
    static constexpr int kWindowSize = 6;
    static constexpr int kBaseMs = 250;
};

#endif // DSEDITORLITE_SMOOTHSCROLLER_H
