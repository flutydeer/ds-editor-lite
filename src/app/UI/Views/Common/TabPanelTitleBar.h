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
    Q_PROPERTY(QColor iconColor READ iconColor WRITE setIconColor)
    Q_PROPERTY(QColor iconDisabledColor READ iconDisabledColor WRITE setIconDisabledColor)
    Q_PROPERTY(QColor iconOnColor READ iconOnColor WRITE setIconOnColor)
    Q_PROPERTY(QColor iconOnDisabledColor READ iconOnDisabledColor WRITE setIconOnDisabledColor)

public:
    explicit TabPanelTitleBar(QWidget *parent = nullptr);

    [[nodiscard]] QTabBar *const &tabBar() const;
    [[nodiscard]] QStackedWidget *const &toolBar() const;

    [[nodiscard]] Button *minimizeButton() const;
    [[nodiscard]] Button *maximizeButton() const;
    [[nodiscard]] Button *closeButton() const;

    void setDetached(bool detached, bool useNativeFrame);
    void setDetachButtonVisible(bool visible);

signals:
    void detachRequested();

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void retranslateUi();
    void buildDockedButtons();
    void buildDetachedButtons(bool useNativeFrame);
    void clearButtonLayout();
    void setActiveStyle(bool active) const;
    // Re-tint docked button icons from the current theme colors
    void rebuildIcons();

    // Theme color accessors (QSS-overridable via qproperty-*); setters
    // re-tint the already-generated icons
    [[nodiscard]] QColor iconColor() const;
    void setIconColor(const QColor &color);
    [[nodiscard]] QColor iconDisabledColor() const;
    void setIconDisabledColor(const QColor &color);
    [[nodiscard]] QColor iconOnColor() const;
    void setIconOnColor(const QColor &color);
    [[nodiscard]] QColor iconOnDisabledColor() const;
    void setIconOnDisabledColor(const QColor &color);

    QColor m_iconColor = {240, 240, 240};
    QColor m_iconDisabledColor = {240, 240, 240, 102};
    QColor m_iconOnColor = {155, 186, 255};
    QColor m_iconOnDisabledColor = {155, 186, 255, 102};

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

#endif // TABPANELTITLEBAR_H
