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
                              const QColor &normalColor,
                              const QColor &disabledColor = QColor());

    QPixmap renderTintedSvgPixmap(const QString &svgPath, const QSize &iconSize,
                                  const QColor &color, qreal devicePixelRatio);

}

#endif // ICONUTILS_H
