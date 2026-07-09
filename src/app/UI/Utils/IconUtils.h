#ifndef ICONUTILS_H
#define ICONUTILS_H

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QString>

namespace IconUtils {

    struct SvgIconColorPalette {
        QColor normal;
        QColor disabled;
        QColor active;
        QColor selected;
    };

    struct SvgIconToggleColorPalette {
        SvgIconColorPalette off;
        SvgIconColorPalette on;
    };

    QIcon createTintedSvgIcon(const QString &svgPath, const QSize &iconSize,
                              const SvgIconColorPalette &palette);
    QIcon createTintedSvgIcon(const QString &svgPath, const QSize &iconSize,
                              const SvgIconToggleColorPalette &palette);

    QIcon createTintedSvgIcon(const QString &svgPath, const QSize &iconSize,
                              const QColor &normalColor, const QColor &disabledColor = QColor());

    QPixmap renderTintedSvgPixmap(const QString &svgPath, const QSize &iconSize,
                                  const QColor &color, qreal devicePixelRatio);

    // Default palette helpers (previously in MainTitleBarIconPalette)
    SvgIconColorPalette defaultActionPalette();
    SvgIconToggleColorPalette defaultToggledPalette(const QColor &checkedColor);

    // Convenience: 16x16 menu icon with default action palette (normal/disabled)
    inline QIcon menuIcon(const QString &svgPath) {
        return createTintedSvgIcon(svgPath, QSize(16, 16), defaultActionPalette());
    }

}

#endif // ICONUTILS_H
