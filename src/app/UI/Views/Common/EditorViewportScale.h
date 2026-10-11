#ifndef EDITORVIEWPORTSCALE_H
#define EDITORVIEWPORTSCALE_H

#include <algorithm>
#include <cmath>

namespace EditorViewportScale {
    inline bool isUsable(double value) {
        return std::isfinite(value) && value > 0.0;
    }

    inline double effectiveMinimum(bool fill, double viewportExtent, double contentExtent,
                                   double minimum, double maximum) {
        // Measure unscaled content, excluding fixed margins; empty content has no fill constraint.
        if (!fill || !isUsable(viewportExtent) || !isUsable(contentExtent))
            return minimum;
        return std::clamp(viewportExtent / std::max(1.0, contentExtent), minimum, maximum);
    }

    inline double bounded(double requested, double current, double minimum, double maximum) {
        const auto fallback = isUsable(current) ? current : 1.0;
        return std::clamp(isUsable(requested) ? requested : fallback, minimum, maximum);
    }
}

#endif // EDITORVIEWPORTSCALE_H
