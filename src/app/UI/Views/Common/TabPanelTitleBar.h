#ifndef TABPANELTITLEBAR_H
#define TABPANELTITLEBAR_H

#include <lite/GUI/Animation/IAnimatable.h>

#include <QWidget>

class Button;
class SegmentedTabBar;
class QStackedWidget;
class QHBoxLayout;
class QGraphicsOpacityEffect;
class QVariantAnimation;

class TabPanelTitleBar : public QWidget, public IAnimatable {
    Q_OBJECT
    Q_PROPERTY(QColor iconColor READ iconColor WRITE setIconColor)
    Q_PROPERTY(QColor iconDisabledColor READ iconDisabledColor WRITE setIconDisabledColor)
    Q_PROPERTY(QColor iconOnColor READ iconOnColor WRITE setIconOnColor)
    Q_PROPERTY(QColor iconOnDisabledColor READ iconOnDisabledColor WRITE setIconOnDisabledColor)

public:
    explicit TabPanelTitleBar(QWidget *parent = nullptr);

    [[nodiscard]] SegmentedTabBar *const &tabBar() const;
    [[nodiscard]] QStackedWidget *const &toolBar() const;

    void setDetached(bool detached);
    void setDetachButtonVisible(bool visible);

signals:
    void detachRequested();

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void afterSetAnimationEnabled(bool enabled) override;
    void afterSetTimeScale(double scale) override;

private:
    void retranslateUi();
    void buildDockedButtons();
    void updateButtonVisibility();
    void setActiveStyle(bool active);
    void updateAnimationSettings();
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

    SegmentedTabBar *m_tabBar;
    QStackedWidget *m_toolBar;
    QHBoxLayout *m_btnLayout;

    Button *m_btnDetach = nullptr;
    Button *m_btnMaximize = nullptr;
    Button *m_btnHide = nullptr;

    bool m_detached = false;
    bool m_detachEnabled = true;
    QGraphicsOpacityEffect *m_opacityEffect = nullptr;
    QVariantAnimation *m_animation = nullptr;
    bool m_targetActive = true;
};

#endif // TABPANELTITLEBAR_H
