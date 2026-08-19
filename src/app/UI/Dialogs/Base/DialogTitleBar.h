#ifndef DIALOGTITLEBAR_H
#define DIALOGTITLEBAR_H

#include <lite/GUI/Animation/IAnimatable.h>

#include <QWidget>

class QVariantAnimation;
class QGraphicsOpacityEffect;
class QLabel;
class Button;

class DialogTitleBar : public QWidget, public IAnimatable {
    Q_OBJECT

public:
    explicit DialogTitleBar(QWidget *parent = nullptr);

    [[nodiscard]] Button *closeButton() const;

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
    Button *m_btnClose = nullptr;
    QGraphicsOpacityEffect *m_opacityEffect;
    QVariantAnimation *m_animation;
    bool m_targetActive = true;
};

#endif // DIALOGTITLEBAR_H
