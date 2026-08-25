#ifndef MAINTITLEBAR_H
#define MAINTITLEBAR_H

#include <lite/GUI/Animation/IAnimatable.h>

#include <QWidget>

class QVariantAnimation;
class QGraphicsOpacityEffect;
class TitleBarComboBox;
class ActionButtonsView;
class PlaybackView;
class SystemWindowButton;
class MainMenuView;

class MainTitleBar : public QWidget, public IAnimatable {
    Q_OBJECT

public:
    explicit MainTitleBar(MainMenuView *menuView, QWidget *parent, bool useNativeFrame);

    [[nodiscard]] MainMenuView *menuView() const;
    [[nodiscard]] ActionButtonsView *actionButtonsView() const;
    [[nodiscard]] PlaybackView *playbackView() const;
    [[nodiscard]] SystemWindowButton *minimizeButton() const;
    [[nodiscard]] SystemWindowButton *maximizeButton() const;
    [[nodiscard]] SystemWindowButton *closeButton() const;

    [[nodiscard]] TitleBarComboBox *titleComboBox() const;
    void setTitle(const QString &title) const;

signals:
    void minimizeTriggered();
    void maximizeTriggered(bool max = false);
    void closeTriggered();

protected:
    void afterSetAnimationEnabled(bool enabled) override;
    void afterSetTimeScale(double scale) override;

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void setActiveStyle(bool active);
    void updateAnimationSettings();
    // Tint the system button icons from the current theme (non-Windows only)
    void rebuildSystemButtonIcons();

    QWidget *m_window;
    MainMenuView *m_menuView;
    ActionButtonsView *m_actionButtonsView;
    PlaybackView *m_playbackView;
    SystemWindowButton *m_btnMin = nullptr;
    SystemWindowButton *m_btnMax = nullptr;
    SystemWindowButton *m_btnClose = nullptr;
    TitleBarComboBox *m_titleComboBox = nullptr;
    QGraphicsOpacityEffect *m_opacityEffect;
    QVariantAnimation *m_animation;
    bool m_targetActive = true;
};



#endif // MAINTITLEBAR_H
