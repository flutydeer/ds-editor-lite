#ifndef TOOLBUTTON_H
#define TOOLBUTTON_H

#include <lite/GUI/Utils/IconUtils.h>

#include <QEnterEvent>
#include <QEvent>
#include <QPushButton>
#include <QSize>

class ToolButton : public QPushButton {
    Q_OBJECT
public:
    explicit ToolButton(QWidget *parent = nullptr);

    void setActionIcon(const QString &svgPath, const QSize &iconSize = QSize(16, 16));
    /// If set, overrides the default action icon normal color (default: icon.primary).
    void setActionIconColor(const QColor &color);
    /// If set, the action icon re-tints to this color while the mouse is over the button.
    void setActionIconHoverColor(const QColor &hoverColor);
    void setToggleIcon(const QString &svgPath, const QSize &iconSize, const QColor &checkedColor);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    enum class IconType { None, Action, Toggle };

    void rebuildIcon();

    IconType m_iconType = IconType::None;
    QString m_svgPath;
    QSize m_actionIconSize;
    QColor m_checkedColor;
    QColor m_actionColor;
    QColor m_actionHoverColor;
    bool m_hovered = false;
};

#endif // TOOLBUTTON_H
