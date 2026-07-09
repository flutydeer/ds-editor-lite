#ifndef MAINTITLEBARICONPALETTE_H
#define MAINTITLEBARICONPALETTE_H

#include "UI/Utils/IconUtils.h"

#include <QColor>

namespace MainTitleBarIconPalette {

    inline IconUtils::SvgIconColorPalette actionPalette() {
        return IconUtils::defaultActionPalette();
    }

    inline IconUtils::SvgIconToggleColorPalette toggledPalette(const QColor &checkedColor) {
        return IconUtils::defaultToggledPalette(checkedColor);
    }

}

#endif // MAINTITLEBARICONPALETTE_H