#ifndef SYSTEMWINDOWBUTTON_H
#define SYSTEMWINDOWBUTTON_H

#include <lite/GUI/Animation/IAnimatable.h>
#include <lite/GUI/Controls/Button.h>

#include <QColor>

class QVariantAnimation;

/// Title-bar system button (minimize / maximize / close) with an
/// overlay-scrollbar-style animated hover/pressed background.
///
/// The background fill is drawn by this widget in paintEvent and driven by a
/// QVariantAnimation, so the QSS must keep `background: transparent` and must
/// NOT define `:hover`/`:pressed` background rules (they would override the
/// animated fill with an instant switch). Token colors follow the close button
/// (window.closeButton.*) or the generic control set (control.fill.*).
class SystemWindowButton : public Button, public IAnimatable {
    Q_OBJECT
public:
    explicit SystemWindowButton(QWidget *parent = nullptr);

    /// Whether to use the close-button token set (window.closeButton.*) instead
    /// of the generic control.fill.* set used by the min/max buttons.
    void setCloseStyle(bool close);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void afterSetAnimationEnabled(bool enabled) override;
    void afterSetTimeScale(double scale) override;

private:
    void updateVisualState();
    void animateTo(qreal target);
    void refreshColors();

    QVariantAnimation *m_animation = nullptr;
    qreal m_progress = 0.0;
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_close = false;
    QColor m_hoverColor;
    QColor m_pressedColor;
};

#endif // SYSTEMWINDOWBUTTON_H
