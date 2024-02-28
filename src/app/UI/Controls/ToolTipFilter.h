//
// Created by fluty on 24-2-22.
//

#ifndef TOOLTIPFILTER_H
#define TOOLTIPFILTER_H

#include <QTimer>

class ToolTip;
class QPropertyAnimation;

class ToolTipFilter : public QObject {

public:
    explicit ToolTipFilter(QWidget *parent, int showDelay = 1000, bool followCursor = false,
                           bool animation = true);
    ~ToolTipFilter() override;

    [[nodiscard]] int showDelay() const;
    void setShowDelay(int delay);
    [[nodiscard]] bool followCursor() const;
    void setFollowCursor(bool on);
    [[nodiscard]] bool animation() const;
    void setAnimation(bool on);
    [[nodiscard]] QString title() const;
    void setTitle(const QString &text);
    [[nodiscard]] QString shortcutKey() const;
    void setShortcutKey(const QString &text);
    [[nodiscard]] QList<QString> message() const;
    void setMessage(const QList<QString> &text);
    void appendMessage(const QString &text);
    void clearMessage();

protected:
    QTimer m_timer;
    ToolTip *m_tooltip;
    QWidget *m_parent;
    int m_showDelay;
    bool m_followCursor;
    bool m_animation;
    bool mouseInParent = false;

    // Animation
    QPropertyAnimation *m_opacityAnimation;

    void adjustToolTipPos();
    void showToolTip();
    void hideToolTip();

    bool eventFilter(QObject *object, QEvent *event) override;
};



#endif //TOOLTIPFILTER_H
