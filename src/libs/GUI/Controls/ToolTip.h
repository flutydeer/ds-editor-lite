#ifndef DATASET_TOOLS_TOOLTIP_H
#define DATASET_TOOLS_TOOLTIP_H

#include <lite/GUI/Animation/IAnimatable.h>

#include <QFrame>

class QLabel;
class QVBoxLayout;
class QPropertyAnimation;
class QGraphicsDropShadowEffect;
class QScreen;

class ToolTip : public QFrame, public IAnimatable {
    Q_OBJECT
    Q_PROPERTY(QColor shadowColor READ shadowColor WRITE setShadowColor)

public:
    explicit ToolTip(const QString &title = "", QWidget *parent = nullptr);
    ~ToolTip() override;

    [[nodiscard]] QString title() const;
    void setTitle(const QString &text);
    [[nodiscard]] QString shortcutKey() const;
    void setShortcutKey(const QString &text);
    [[nodiscard]] QList<QString> message() const;
    void setMessage(const QList<QString> &text);
    void appendMessage(const QString &text);
    void clearMessage();

    void setAnimationEnabled(bool on);
    [[nodiscard]] bool animationEnabled() const;
    void showAt(const QPoint &screenPos);
    void showAbove(const QRect &screenRect);
    void moveTo(const QPoint &screenPos);
    void hideWithAnimation();

signals:
    void hideAnimationFinished();

protected:
    void afterSetAnimationEnabled(bool enabled) override;
    void afterSetTimeScale(double scale) override;

    QString m_title;
    QString m_shortcutKey;
    QList<QString> m_message;

    QLabel *m_lbTitle;
    QLabel *m_lbShortcutKey;
    QVBoxLayout *m_cardLayout;
    QVBoxLayout *m_messageLayout;

    QPropertyAnimation *m_opacityAnimation;
    QGraphicsDropShadowEffect *m_shadowEffect;
    bool m_animationEnabled = true;

    void updateMessage();
    void showAt(const QPoint &screenPos, const QScreen *screen);
    [[nodiscard]] QPoint clampToScreen(const QPoint &screenPos,
                                       const QScreen *screen = nullptr) const;
    [[nodiscard]] QColor shadowColor() const;
    void setShadowColor(const QColor &color);
    void updateAnimationSettings();
    void completeOpacityAnimation();
};

#endif // DATASET_TOOLS_TOOLTIP_H
