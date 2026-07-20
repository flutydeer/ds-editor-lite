#ifndef MAINTITLEBARICONPALETTE_H
#define MAINTITLEBARICONPALETTE_H

#include "UI/Utils/IconUtils.h"

#include <QColor>

namespace MainTitleBarIconPalette {

    inline IconUtils::SvgIconColorPalette actionPalette(const QColor &normalColor,
                                                        const QColor &disabledColor) {
        IconUtils::SvgIconColorPalette palette;
        palette.normal = normalColor;
        palette.disabled = disabledColor;
        return palette;
    }

    inline IconUtils::SvgIconToggleColorPalette toggledPalette(const QColor &normalColor,
                                                               const QColor &disabledColor,
                                                               const QColor &checkedColor) {
        IconUtils::SvgIconToggleColorPalette palette;
        palette.off = actionPalette(normalColor, disabledColor);
        palette.on.normal = checkedColor;
        palette.on.disabled = disabledColor;
        return palette;
    }

}

#endif // MAINTITLEBARICONPALETTE_H
