#ifndef NOTELYRICPRESENTATION_H
#define NOTELYRICPRESENTATION_H

#include "UI/Views/Common/EditorItemGeometry.h"

#include <QFont>
#include <QFontMetricsF>
#include <QRectF>
#include <QString>

#include <algorithm>
#include <cmath>

namespace NoteLyricPresentation {
    inline constexpr double compactScaleThreshold = 0.3;
    inline constexpr double horizontalPadding = 2.0;

    struct Layout {
        QRectF textRect;
        QString displayText;
        bool elided = false;

        [[nodiscard]] bool isVisible() const {
            return !displayText.isEmpty();
        }
    };

    inline bool usesCompactRendering(const double horizontalScale) {
        return horizontalScale < compactScaleThreshold;
    }

    inline QRectF textRect(const QRectF &noteRect) {
        return EditorItemGeometry::notePaintRect(noteRect).adjusted(horizontalPadding, 0.0,
                                                                    -horizontalPadding, 0.0);
    }

    inline Layout layout(const QRectF &noteRect, const QString &lyric, const QFont &font,
                         const double horizontalScale) {
        Layout result;
        result.textRect = textRect(noteRect);
        if (lyric.isEmpty() || usesCompactRendering(horizontalScale) ||
            result.textRect.height() <= 0.0) {
            return result;
        }

        const QFontMetricsF metrics(font);
        if (metrics.height() >= result.textRect.height())
            return result;

        const auto availableWidth =
            static_cast<int>(std::floor(std::max(0.0, result.textRect.width())));
        result.displayText = metrics.elidedText(lyric, Qt::ElideRight, availableWidth);
        result.elided = result.displayText != lyric;
        return result;
    }

    inline bool isElidedInRect(const Layout &layout, const QString &lyric, const QFont &font,
                               const QRectF &visibleRect) {
        if (layout.elided || !layout.isVisible())
            return layout.elided;

        const QFontMetricsF metrics(font);
        const QRectF textBounds(
            layout.textRect.left(),
            layout.textRect.top() + (layout.textRect.height() - metrics.height()) * 0.5,
            metrics.horizontalAdvance(lyric), metrics.height());
        return !visibleRect.contains(textBounds);
    }
}

#endif // NOTELYRICPRESENTATION_H
