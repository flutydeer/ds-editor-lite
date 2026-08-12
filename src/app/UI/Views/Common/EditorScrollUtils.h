#ifndef EDITORSCROLLUTILS_H
#define EDITORSCROLLUTILS_H

#include <QtMath>

#include <algorithm>

namespace EditorScrollUtils {

    inline int rangeMaximum(const double contentLength, const double viewportLength) {
        return qRound(std::max(0.0, contentLength - viewportLength));
    }

    inline int boundedOffset(const double requestedOffset, const double contentLength,
                             const double viewportLength) {
        return std::clamp(qRound(requestedOffset), 0, rangeMaximum(contentLength, viewportLength));
    }

} // namespace EditorScrollUtils

#endif // EDITORSCROLLUTILS_H
