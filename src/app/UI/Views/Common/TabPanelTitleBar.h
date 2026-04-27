//
// Created by FlutyDeer on 2025/7/13.
//

#ifndef TABPANELTITLEBAR_H
#define TABPANELTITLEBAR_H

#include <QWidget>

class Button;
class QTabBar;
class QStackedWidget;
class QHBoxLayout;
class QGraphicsOpacityEffect;
class QVariantAnimation;

class TabPanelTitleBar : public QWidget {
    Q_OBJECT

public:
    explicit TabPanelTitleBar(QWidget *parent = nullptr);

    [[nodiscard]] QTabBar *const &tabBar() const;
    [[nodiscard]] QStackedWidget *const &toolBar() const;

    [[nodiscard]] Button *minimizeButton() const;
    [[nodiscard]] Button *maximizeButton() const;
    [[nodiscard]] Button *closeButton() const;

    void setDetached(bool detached, bool useNativeFrame);

signals:
    void detachRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildDockedButtons();
    void buildDetachedButtons(bool useNativeFrame);
    void clearButtonLayout();
    void setActiveStyle(bool active) const;

    QTabBar *m_tabBar;
    QStackedWidget *m_toolBar;
    QHBoxLayout *m_outerLayout;
    QHBoxLayout *m_innerLayout;
    QHBoxLayout *m_btnLayout;
    QHBoxLayout *m_systemBtnLayout;

    Button *m_btnDetach = nullptr;
    Button *m_btnMaximize = nullptr;
    Button *m_btnHide = nullptr;

    Button *m_btnMin = nullptr;
    Button *m_btnMax = nullptr;
    Button *m_btnClose = nullptr;

    bool m_detached = false;
    QGraphicsOpacityEffect *m_opacityEffect = nullptr;
    QVariantAnimation *m_animation = nullptr;
};

#endif //TABPANELTITLEBAR_H
