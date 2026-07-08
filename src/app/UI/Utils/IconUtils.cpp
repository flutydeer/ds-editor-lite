#include "IconUtils.h"

#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QPixmapCache>
#include <QSvgRenderer>
#include <QtMath>
#include <array>

namespace {

    constexpr std::array<qreal, 2> kDevicePixelRatios{1.0, 2.0};
    constexpr std::array<QIcon::Mode, 4> kIconModes{
        QIcon::Normal,
        QIcon::Disabled,
        QIcon::Active,
        QIcon::Selected,
    };

    QString pixmapCacheKey(const QString &svgPath, const QSize &iconSize, const QColor &color,
                           qreal devicePixelRatio) {
        return QStringLiteral("IconUtils:tinted-svg:%1:%2x%3:%4:%5")
            .arg(svgPath)
            .arg(iconSize.width())
            .arg(iconSize.height())
            .arg(QString::number(devicePixelRatio, 'f', 3))
            .arg(color.isValid() ? color.name(QColor::HexArgb) : QStringLiteral("none"));
    }

    QPixmap renderSvgUncached(const QString &svgPath, const QSize &iconSize, const QColor &color,
                              qreal devicePixelRatio) {
        QSvgRenderer renderer(svgPath);
        if (!renderer.isValid() || !iconSize.isValid()) {
            return {};
        }

        const QSize pixelSize = QSize(qRound(iconSize.width() * devicePixelRatio),
                                      qRound(iconSize.height() * devicePixelRatio));
        if (pixelSize.isEmpty()) {
            return {};
        }

        QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        renderer.render(&painter, QRect(QPoint(0, 0), pixelSize));

        if (color.isValid()) {
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(image.rect(), color);
        }

        painter.end();

        QPixmap pixmap = QPixmap::fromImage(image);
        pixmap.setDevicePixelRatio(devicePixelRatio);
        return pixmap;
    }

    QColor resolveModeColor(const IconUtils::SvgIconColorPalette &palette, QIcon::Mode mode) {
        const QColor fallback = palette.normal;
        switch (mode) {
            case QIcon::Disabled:
                return palette.disabled.isValid() ? palette.disabled : fallback;
            case QIcon::Active:
                return palette.active.isValid() ? palette.active : fallback;
            case QIcon::Selected:
                return palette.selected.isValid() ? palette.selected : fallback;
            case QIcon::Normal:
            default:
                return fallback;
        }
    }

    IconUtils::SvgIconColorPalette normalizePalette(const IconUtils::SvgIconColorPalette &palette,
                                                    const QColor &fallbackNormal = QColor()) {
        IconUtils::SvgIconColorPalette result;
        result.normal = palette.normal.isValid() ? palette.normal : fallbackNormal;
        if (!result.normal.isValid()) {
            return result;
        }

        result.disabled = palette.disabled.isValid() ? palette.disabled : result.normal;
        result.active = palette.active.isValid() ? palette.active : result.normal;
        result.selected = palette.selected.isValid() ? palette.selected : result.normal;
        return result;
    }

    void addModePixmaps(QIcon &icon, const QString &svgPath, const QSize &iconSize,
                        QIcon::Mode mode, QIcon::State state, const QColor &color) {
        for (const auto dpr : kDevicePixelRatios) {
            const auto pixmap = IconUtils::renderTintedSvgPixmap(svgPath, iconSize, color, dpr);
            if (pixmap.isNull()) {
                continue;
            }
            icon.addPixmap(pixmap, mode, state);
        }
    }

    void addPalettePixmaps(QIcon &icon, const QString &svgPath, const QSize &iconSize,
                           QIcon::State state,
                           const IconUtils::SvgIconColorPalette &normalizedPalette) {
        for (const auto mode : kIconModes) {
            addModePixmaps(icon, svgPath, iconSize, mode, state,
                           resolveModeColor(normalizedPalette, mode));
        }
    }

}

namespace IconUtils {

    QPixmap renderTintedSvgPixmap(const QString &svgPath, const QSize &iconSize,
                                  const QColor &color, const qreal devicePixelRatio) {
        if (svgPath.isEmpty() || !iconSize.isValid() || devicePixelRatio <= 0) {
            return {};
        }

        const auto key = pixmapCacheKey(svgPath, iconSize, color, devicePixelRatio);
        QPixmap pixmap;
        if (QPixmapCache::find(key, &pixmap)) {
            return pixmap;
        }

        pixmap = renderSvgUncached(svgPath, iconSize, color, devicePixelRatio);
        if (!pixmap.isNull()) {
            QPixmapCache::insert(key, pixmap);
        }
        return pixmap;
    }

    QIcon createTintedSvgIcon(const QString &svgPath, const QSize &iconSize,
                              const SvgIconColorPalette &palette) {
        if (svgPath.isEmpty() || !iconSize.isValid()) {
            return {};
        }

        const auto normalizedPalette = normalizePalette(palette);
        if (!normalizedPalette.normal.isValid()) {
            return QIcon(svgPath);
        }

        QIcon icon;
        addPalettePixmaps(icon, svgPath, iconSize, QIcon::Off, normalizedPalette);
        addPalettePixmaps(icon, svgPath, iconSize, QIcon::On, normalizedPalette);
        return icon;
    }

    QIcon createTintedSvgIcon(const QString &svgPath, const QSize &iconSize,
                              const SvgIconToggleColorPalette &palette) {
        if (svgPath.isEmpty() || !iconSize.isValid()) {
            return {};
        }

        const auto offPalette = normalizePalette(palette.off, palette.on.normal);
        const auto onPalette = normalizePalette(palette.on, offPalette.normal);
        if (!offPalette.normal.isValid() || !onPalette.normal.isValid()) {
            return QIcon(svgPath);
        }

        QIcon icon;
        addPalettePixmaps(icon, svgPath, iconSize, QIcon::Off, offPalette);
        addPalettePixmaps(icon, svgPath, iconSize, QIcon::On, onPalette);
        return icon;
    }

    QIcon createTintedSvgIcon(const QString &svgPath, const QSize &iconSize,
                              const QColor &normalColor, const QColor &disabledColor) {
        SvgIconColorPalette palette;
        palette.normal = normalColor;
        palette.disabled = disabledColor;
        return createTintedSvgIcon(svgPath, iconSize, palette);
    }

}
