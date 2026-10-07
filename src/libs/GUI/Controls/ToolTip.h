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
    // Anchors the visible card directly above `screenPos`, horizontally centred
    // on it, with enough clearance to keep a fingertip or a pen nib from
    // covering it. Used by drag interactions: a tooltip placed next to the
    // pointer is always hidden under the finger holding the pointer there.
    void showAbovePointer(const QPoint &screenPos, const QScreen *screen = nullptr);
    void moveAbovePointer(const QPoint &screenPos, const QScreen *screen = nullptr);
    // How far above the pointer the card is kept, in logical pixels, for the
    // given screen (the primary screen when null). Derived from the screen's
    // physical dots per inch, so it stays a real-world distance on every DPI.
    [[nodiscard]] static int pointerClearance(const QScreen *screen = nullptr);
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
    // Sizes the window to the current content hint in both directions. The
    // card is reused across shows: a content change leaves the top-level
    // layout's cached hint stale until the deferred LayoutRequest lands, and
    // wider content leaves its size constraints behind, so the layout is
    // reactivated synchronously and min=max are pinned to the fresh hint
    void resizeToContentHint();
    // Top-left corner of the widget that puts its visible card `gapPx` above
    // `anchorPos` and centred on it. Shared by the rect anchor (hover tooltips)
    // and the pointer anchor (drag tooltips); only the gap differs.
    [[nodiscard]] QPoint positionAbove(const QPoint &anchorPos, const QScreen *screen,
                                       int gapPx) const;
    [[nodiscard]] static const QScreen *resolveScreen(const QPoint &screenPos,
                                                      const QScreen *screen);
    [[nodiscard]] QPoint clampToScreen(const QPoint &screenPos,
                                       const QScreen *screen = nullptr) const;
    [[nodiscard]] QColor shadowColor() const;
    void setShadowColor(const QColor &color);
    void updateAnimationSettings();
    void completeOpacityAnimation();
};

#endif // DATASET_TOOLS_TOOLTIP_H
