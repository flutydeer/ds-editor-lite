//
// Created by fluty on 24-7-30.
//

#ifndef MAINTITLEBAR_H
#define MAINTITLEBAR_H

#include <QWidget>

class QGraphicsOpacityEffect;
class QLabel;
class ActionButtonsView;
class PlaybackView;
class Button;
class MainMenuView;
class MainTitleBar : public QWidget {
    Q_OBJECT

public:
    explicit MainTitleBar(MainMenuView *menuView, QWidget *parent, bool useNativeFrame);

    [[nodiscard]] MainMenuView *menuView() const;
    [[nodiscard]] ActionButtonsView *actionButtonsView() const;
    [[nodiscard]] PlaybackView *playbackView() const;
    [[nodiscard]] Button *minimizeButton() const;
    [[nodiscard]] Button *maximizeButton() const;
    [[nodiscard]] Button *closeButton() const;

    void setTitle(const QString &title) const;

signals:
    void minimizeTriggered();
    void maximizeTriggered(bool max = false);
    void closeTriggered();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void setActiveStyle(bool active);

    QWidget *m_window;
    MainMenuView *m_menuView;
    ActionButtonsView *m_actionButtonsView;
    PlaybackView *m_playbackView;
    Button *m_btnMin = nullptr;
    Button *m_btnMax = nullptr;
    Button *m_btnClose = nullptr;
    QLabel *m_lbTitle = nullptr;
    QGraphicsOpacityEffect *m_opacityEffect;
};



#endif // MAINTITLEBAR_H
