#include <lite/GUI/Controls/ToolButton.h>

#include <lite/GUI/Utils/IconUtils.h>
#include <lite/GUI/Theme/ThemeManager.h>

ToolButton::ToolButton(QWidget *parent) : QPushButton(parent) {
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, &ToolButton::rebuildIcon);
}

void ToolButton::setActionIcon(const QString &svgPath, const QSize &iconSize) {
    m_iconType = IconType::Action;
    m_svgPath = svgPath;
    m_actionIconSize = iconSize;
    m_checkedColor = {};
    m_hovered = false;
    rebuildIcon();
}

void ToolButton::setActionIconColor(const QColor &color) {
    if (m_actionColor == color)
        return;
    m_actionColor = color;
    if (m_iconType == IconType::Action)
        rebuildIcon();
}

void ToolButton::setActionIconHoverColor(const QColor &hoverColor) {
    if (m_actionHoverColor == hoverColor)
        return;
    m_actionHoverColor = hoverColor;
    if (m_iconType == IconType::Action)
        rebuildIcon();
}

void ToolButton::setToggleIcon(const QString &svgPath, const QSize &iconSize,
                               const QColor &checkedColor) {
    m_iconType = IconType::Toggle;
    m_svgPath = svgPath;
    m_actionIconSize = iconSize;
    m_checkedColor = checkedColor;
    rebuildIcon();
}

void ToolButton::rebuildIcon() {
    if (m_iconType == IconType::None)
        return;

    setIconSize(m_actionIconSize);
    if (m_iconType == IconType::Toggle) {
        setIcon(IconUtils::createTintedSvgIcon(m_svgPath, m_actionIconSize,
                                               IconUtils::defaultToggledPalette(m_checkedColor)));
        return;
    }

    auto palette = IconUtils::defaultActionPalette();
    if (m_actionColor.isValid()) {
        palette.normal = m_actionColor;
        palette.active = m_actionColor;
        palette.selected = m_actionColor;
    }
    if (m_actionHoverColor.isValid() && m_hovered) {
        // Keep the palette's disabled tone; only brighten the active/normal modes.
        palette.normal = m_actionHoverColor;
        palette.active = m_actionHoverColor;
        palette.selected = m_actionHoverColor;
    }
    setIcon(IconUtils::createTintedSvgIcon(m_svgPath, m_actionIconSize, palette));
}

void ToolButton::enterEvent(QEnterEvent *event) {
    QPushButton::enterEvent(event);
    if (m_iconType == IconType::Action && m_actionHoverColor.isValid() && !m_hovered) {
        m_hovered = true;
        rebuildIcon();
    }
}

void ToolButton::leaveEvent(QEvent *event) {
    QPushButton::leaveEvent(event);
    if (m_iconType == IconType::Action && m_actionHoverColor.isValid() && m_hovered) {
        m_hovered = false;
        rebuildIcon();
    }
}
