#include <lite/GUI/Controls/ToolButton.h>

#include <lite/GUI/Utils/IconUtils.h>
#include <lite/GUI/Theme/ThemeManager.h>

ToolButton::ToolButton(QWidget *parent) : QPushButton(parent) {
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
            &ToolButton::rebuildIcon);
}

void ToolButton::setActionIcon(const QString &svgPath, const QSize &iconSize) {
    m_iconType = IconType::Action;
    m_svgPath = svgPath;
    m_actionIconSize = iconSize;
    m_checkedColor = {};
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
        setIcon(IconUtils::createTintedSvgIcon(
            m_svgPath, m_actionIconSize, IconUtils::defaultToggledPalette(m_checkedColor)));
        return;
    }

    setIcon(IconUtils::createTintedSvgIcon(m_svgPath, m_actionIconSize,
                                           IconUtils::defaultActionPalette()));
}
