//
// Created by FlutyDeer on 2026/7/9.
//

#include "ToolButton.h"

#include "UI/Utils/IconUtils.h"

ToolButton::ToolButton(QWidget *parent) : QPushButton(parent) {
}

void ToolButton::setActionIcon(const QString &svgPath, const QSize &iconSize) {
    setIconSize(iconSize);
    setIcon(IconUtils::createTintedSvgIcon(svgPath, iconSize, IconUtils::defaultActionPalette()));
}

void ToolButton::setToggleIcon(const QString &svgPath, const QSize &iconSize,
                               const QColor &checkedColor) {
    setIconSize(iconSize);
    setIcon(IconUtils::createTintedSvgIcon(svgPath, iconSize,
                                           IconUtils::defaultToggledPalette(checkedColor)));
}