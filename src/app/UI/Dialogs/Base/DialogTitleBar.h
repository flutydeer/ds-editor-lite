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
    explicit DialogTitleBar(QWidget *parent = nullptr);

    [[nodiscard]] SystemWindowButton *closeButton() const;

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
    SystemWindowButton *m_btnClose = nullptr;
    QGraphicsOpacityEffect *m_opacityEffect;
    QVariantAnimation *m_animation;
    bool m_targetActive = true;
};

#endif // DIALOGTITLEBAR_H
