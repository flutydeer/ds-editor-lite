#ifndef TOAST_H
#define TOAST_H

#include <lite/Core/Singleton.h>
#include <lite/GUI/Animation/IAnimatable.h>
#include <lite/ADT/Queue.h>

#include <QPointer>
#include <QWidget>
#include <QTimer>
#include <QPropertyAnimation>

class QVBoxLayout;
class QLabel;
class QGraphicsDropShadowEffect;

class ToastWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor shadowColor READ shadowColor WRITE setShadowColor)

public:
    explicit ToastWidget(const QString &text, QWidget *parent = nullptr);

private:
    [[nodiscard]] QColor shadowColor() const;
    void setShadowColor(const QColor &color);

    QLabel *m_lbMessage;
    QVBoxLayout *m_cardLayout;
    QGraphicsDropShadowEffect *m_shadowEffect;
};

class Toast : public QObject, public IAnimatable {
    Q_OBJECT

private:
    explicit Toast(QObject *parent = nullptr);
    ~Toast() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(Toast)
    Q_DISABLE_COPY_MOVE(Toast)

public:
    static void setGlobalContext(QWidget *context);
    static void show(const QString &message);

protected:
    void afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) override;
    void afterSetTimeScale(double scale) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void hideToast();

private:
    void showNextToast();
    void oneToastShowFinished();
    void destroyCurrentToast();
    void updateAnimationSettings();
    /// 计算 toast 的目标位置（相对主窗口水平居中，卡片顶部 y=96）
    [[nodiscard]] QPoint targetPos() const;

    bool m_isShowingToast = false;
    const int animationDurationBase = 300;
    static QPointer<QWidget> m_globalContext;
    ToastWidget *m_toastWidget = nullptr;
    Queue<QString> m_queue;
    QTimer m_keepOnScreenTimer;
    QTimer m_destroyWidgetTimer;
    QPropertyAnimation m_opacityAnimation;
    QPropertyAnimation m_posAnimation;
};



#endif // TOAST_H
