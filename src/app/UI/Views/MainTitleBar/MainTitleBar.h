#ifndef MAINTITLEBAR_H
#define MAINTITLEBAR_H

#include "TitleBarSections.h"
#include <lite/GUI/Animation/IAnimatable.h>

#include <QList>
#include <QWidget>

class QVariantAnimation;
class QGraphicsOpacityEffect;
class QHBoxLayout;
class DividerLine;
class TitleBarComboBox;
class ActionButtonsView;
class PlaybackView;
class SystemWindowButton;
class MainMenuView;

class MainTitleBar : public QWidget, public IAnimatable {
    Q_OBJECT

public:
    // The bar is the window chrome for both the main window and a detached
    // panel window. `sections` selects the content, `role` decides whether the
    // transport owns the application-level shortcuts.
    explicit MainTitleBar(MainMenuView *menuView, QWidget *parent,
                          const TitleBarSections &sections,
                          TitleBarRole role = TitleBarRole::Primary);

    void setInWindowMenu(bool inWindow);
    [[nodiscard]] const TitleBarSections &sections() const;
    [[nodiscard]] QList<DividerLine *> dividers() const;

    [[nodiscard]] MainMenuView *menuView() const;
    [[nodiscard]] QWidget *trafficLightArea() const;
    [[nodiscard]] ActionButtonsView *actionButtonsView() const;
    [[nodiscard]] PlaybackView *playbackView() const;
    [[nodiscard]] SystemWindowButton *minimizeButton() const;
    [[nodiscard]] SystemWindowButton *maximizeButton() const;
    [[nodiscard]] SystemWindowButton *closeButton() const;

    [[nodiscard]] TitleBarComboBox *titleComboBox() const;
    void setTitle(const QString &title) const;

    // Returns the system button covering the window-level position pos, with
    // the button hit area extended to the window's right edge. Used to work
    // around the one-logical-pixel gap at the window edge where
    // QWidget::childAt never resolves to any child (non-integral DPR only).
    [[nodiscard]] SystemWindowButton *systemButtonAt(const QPointF &windowPos) const;

signals:
    void minimizeTriggered();
    void maximizeTriggered(bool max = false);
    void closeTriggered();
    void doubleClicked();

protected:
    void afterSetAnimationEnabled(bool enabled) override;
    void afterSetTimeScale(double scale) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void buildSections();
    // Caps the project selector at what the rest of the bar leaves over, so it
    // gives way before the menu bar has to collapse any menu.
    void updateSectionWidths() const;
    void createSystemButtons();
    void setActiveStyle(bool active);
    void updateAnimationSettings();
    // Tint the system button icons from the current theme (non-Windows only)
    void rebuildSystemButtonIcons();

    QWidget *m_window;
    MainMenuView *m_menuView;
    TitleBarSections m_sections;
    QHBoxLayout *m_layout;
    QList<DividerLine *> m_dividers;
    QWidget *m_trafficLightArea = nullptr;
    ActionButtonsView *m_actionButtonsView = nullptr;
    PlaybackView *m_playbackView = nullptr;
    SystemWindowButton *m_btnMin = nullptr;
    SystemWindowButton *m_btnMax = nullptr;
    SystemWindowButton *m_btnClose = nullptr;
    TitleBarComboBox *m_titleComboBox = nullptr;
    QGraphicsOpacityEffect *m_opacityEffect;
    QVariantAnimation *m_animation;
    bool m_targetActive = true;
};

#endif // MAINTITLEBAR_H
