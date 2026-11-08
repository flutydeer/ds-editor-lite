#ifndef DIALOGTITLEBAR_H
#define DIALOGTITLEBAR_H

#include <lite/GUI/Animation/IAnimatable.h>

#include <QWidget>

class QVariantAnimation;
class QGraphicsOpacityEffect;
class QLabel;
class SystemWindowButton;

class DialogTitleBar : public QWidget, public IAnimatable {
    Q_OBJECT

public:
    // trafficLightArea reserves the leading slot for the native macOS traffic
    // lights and drops the custom close button: the platform already draws one
    // there, and a second Windows-style glyph was never right on macOS.
    explicit DialogTitleBar(QWidget *parent = nullptr, bool trafficLightArea = false);

    [[nodiscard]] SystemWindowButton *closeButton() const;
    [[nodiscard]] QWidget *trafficLightArea() const;

    // Returns the close button covering the window-level position pos, with
    // the button hit area extended to the window's right edge. Same rationale
    // as MainTitleBar::systemButtonAt.
    [[nodiscard]] SystemWindowButton *systemButtonAt(const QPointF &windowPos) const;

    void setTitle(const QString &title) const;

signals:
    void closeTriggered();

protected:
    void afterSetAnimationEnabled(bool enabled) override;
    void afterSetTimeScale(double scale) override;

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void setActiveStyle(bool active);
    void updateAnimationSettings();
    // Tint the close button icon from the current theme (non-Windows only)
    void rebuildCloseButtonIcon();

    QWidget *m_window;
    QLabel *m_lbTitle = nullptr;
    QWidget *m_trafficLightArea = nullptr;
    SystemWindowButton *m_btnClose = nullptr;
    QGraphicsOpacityEffect *m_opacityEffect;
    QVariantAnimation *m_animation;
    bool m_targetActive = true;
};

#endif // DIALOGTITLEBAR_H
