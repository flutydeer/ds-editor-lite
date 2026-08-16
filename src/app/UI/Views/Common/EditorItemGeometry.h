#ifndef EDITORITEMGEOMETRY_H
#define EDITORITEMGEOMETRY_H

#include <QRectF>

#include <algorithm>

namespace EditorItemGeometry {
    inline constexpr float noteBorderWidth = 1.5f;
    inline constexpr int noteCornerRadius = 2;
    inline constexpr float noteMinimumVisualWidth = 1.0f;
    inline constexpr float clipBorderWidth = 1.2f;
    inline constexpr float clipHorizontalPadding = clipBorderWidth * 0.5f;
    inline constexpr float clipVerticalPadding = 1.2f;
    inline constexpr int clipCornerRadius = 4;
    inline constexpr float clipMinimumVisualWidth = 1.0f;

    inline QRectF paintRect(const QRectF &modelRect, const double horizontalPadding,
                           const double verticalPadding, const double minimumVisualWidth) {
        if (modelRect.isEmpty() || horizontalPadding < 0.0 || verticalPadding < 0.0 ||
            minimumVisualWidth <= 0.0) {
            return {};
        }
        const auto height = modelRect.height() - verticalPadding * 2.0;
        if (height <= 0.0)
            return {};
        const auto width =
            std::max(minimumVisualWidth, modelRect.width() - horizontalPadding * 2.0);
        return {modelRect.center().x() - width * 0.5, modelRect.top() + verticalPadding, width,
                height};
    }

    inline QRectF notePaintRect(const QRectF &modelRect, const double scale = 1.0) {
        if (scale <= 0.0)
            return {};
        return paintRect(modelRect, noteBorderWidth * scale, noteBorderWidth * scale,
                         noteMinimumVisualWidth * scale);
    }

    inline QRectF clipPaintRect(const QRectF &modelRect, const double scale = 1.0) {
        if (scale <= 0.0)
            return {};
        return paintRect(modelRect, clipHorizontalPadding * scale, clipVerticalPadding * scale,
                         clipMinimumVisualWidth * scale);
    }

    inline double adaptiveCornerRadius(const QRectF &rect, const double cornerRadius) {
        const auto normalizedRect = rect.normalized();
        if (normalizedRect.isEmpty() || cornerRadius <= 0.0)
            return 0.0;
        return std::min(
            {cornerRadius, normalizedRect.width() * 0.5, normalizedRect.height() * 0.5});
    }
}

#endif // EDITORITEMGEOMETRY_H
