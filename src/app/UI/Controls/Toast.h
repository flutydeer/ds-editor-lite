//
// Created by fluty on 2024/7/14.
//

#ifndef TOAST_H
#define TOAST_H

#include "Utils/Singleton.h"
#include "UI/Utils/IAnimatable.h"
#include "Utils/Queue.h"

#include <QWidget>
#include <QTimer>
#include <QPropertyAnimation>

class QVBoxLayout;
class QLabel;

class ToastWidget : public QWidget {
    Q_OBJECT

public:
    explicit ToastWidget(const QString &text, QWidget *parent = nullptr);

private:
    QLabel *m_lbMessage;
    QVBoxLayout *m_cardLayout;
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

private slots:
    void hideToast();

private:
    void showNextToast();
    void oneToastShowFinished();

    bool m_isShowingToast = false;
    const int animationDurationBase = 300;
    static QWidget *m_globalContext;
    ToastWidget *m_toastWidget = nullptr;
    Queue<QString> m_queue;
    QTimer m_keepOnScreenTimer;
    QTimer m_destroyWidgetTimer;
    QPropertyAnimation m_opacityAnimation;
    QPropertyAnimation m_posAnimation;
};



#endif // TOAST_H
