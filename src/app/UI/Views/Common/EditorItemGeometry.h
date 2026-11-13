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
    // Height of the band that carries a clip title, in logical pixels. Both
    // backends read it from here so the painted title, the preview area below
    // it and the inline rename anchor share one number.
    inline constexpr float clipTitleHeight = 20.0f;
    // Preview height below which the title is no longer drawn as its own strip
    // but centred over the whole clip, so the clip has no title band to target.
    inline constexpr float clipTitleStripMinPreviewHeight = 32.0f;

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

    // Uniformly scaled copy. QRectF has no division operator, and every place
    // that mixes physical and logical editor coordinates needs the same
    // conversion.
    inline QRectF scaledRect(const QRectF &rect, const double factor) {
        if (factor == 0.0 || rect.isEmpty())
            return {};
        return {rect.left() * factor, rect.top() * factor, rect.width() * factor,
                rect.height() * factor};
    }

    // The band a clip title sits in, in the clip's paint-rect coordinates. An
    // empty or degenerate rect yields nothing to target.
    inline QRectF clipTitleBand(const QRectF &paintRect) {
        if (paintRect.isEmpty() || paintRect.width() <= 0.0 || paintRect.height() <= 0.0)
            return {};
        return {paintRect.left(), paintRect.top(), paintRect.width(),
                std::min(static_cast<double>(clipTitleHeight), paintRect.height())};
    }

    // Whether the title is drawn as its own strip. Short clips centre their
    // title over the whole body instead, and then the body belongs to the
    // ordinary clip gestures rather than to the inline rename.
    inline bool clipHasTitleStrip(const QRectF &paintRect) {
        if (paintRect.isEmpty() || paintRect.height() <= 0.0)
            return false;
        return paintRect.height() - clipTitleHeight >= clipTitleStripMinPreviewHeight;
    }
}

#endif // EDITORITEMGEOMETRY_H
