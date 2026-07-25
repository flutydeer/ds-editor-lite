//
// Created by fluty on 2023/8/30.
//

#ifndef DATASET_TOOLS_TOOLTIP_H
#define DATASET_TOOLS_TOOLTIP_H

#include <QFrame>

class QLabel;
class QVBoxLayout;
class QPropertyAnimation;
class QGraphicsDropShadowEffect;

class ToolTip : public QFrame {
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
    void moveTo(const QPoint &screenPos);
    void hideWithAnimation();

signals:
    void hideAnimationFinished();

protected:
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
    [[nodiscard]] QPoint clampToScreen(const QPoint &screenPos) const;
    [[nodiscard]] QColor shadowColor() const;
    void setShadowColor(const QColor &color);
};

#endif // DATASET_TOOLS_TOOLTIP_H
