#ifndef EDITORSCROLLUTILS_H
#define EDITORSCROLLUTILS_H

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace EditorScrollUtils {

    inline int rangeMaximum(const double contentLength, const double viewportLength) {
        return qRound(std::max(0.0, contentLength - viewportLength));
    }

    inline int boundedOffset(const double requestedOffset, const double contentLength,
                             const double viewportLength) {
        return std::clamp(qRound(requestedOffset), 0, rangeMaximum(contentLength, viewportLength));
    }

    inline double ensureVisibleOffset(const double currentOffset, const double viewportLength,
                                      const double contentStart, const double contentEnd,
                                      const double margin) {
        if (!std::isfinite(currentOffset) || !std::isfinite(viewportLength) ||
            !std::isfinite(contentStart) || !std::isfinite(contentEnd) || !std::isfinite(margin) ||
            viewportLength <= 0.0) {
            return currentOffset;
        }
        const auto safeMargin = std::max(0.0, margin);
        const auto requiredStart = std::min(contentStart, contentEnd) - safeMargin;
        const auto requiredEnd = std::max(contentStart, contentEnd) + safeMargin;
        if (requiredStart < currentOffset)
            return requiredStart;
        if (requiredEnd > currentOffset + viewportLength)
            return requiredEnd - viewportLength;
        return currentOffset;
    }

} // namespace EditorScrollUtils

#endif // EDITORSCROLLUTILS_H
