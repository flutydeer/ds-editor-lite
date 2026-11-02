#ifndef AUDIOLANEGEOMETRY_H
#define AUDIOLANEGEOMETRY_H

#include <QRectF>

#include <algorithm>

// Vertical layout shared by the audio view canvas and its left axis column.
// Both widgets derive the lane bands from their own height, so the split has to
// live in one place: if the axis used a different band height than the waveform
// it describes, the dB ticks would point at the wrong amplitude.
namespace AudioLaneGeometry {
    inline int laneCount(const int channels) {
        return std::max(1, channels);
    }

    inline double laneHeight(const double totalHeight, const int channels) {
        return totalHeight / laneCount(channels);
    }

    // Lane bands tile the available height, the last one absorbs the rounding
    inline QRectF laneRect(const QRectF &bounds, const int channels, const int lane) {
        if (bounds.isEmpty())
            return {};
        const auto count = laneCount(channels);
        const auto height = laneHeight(bounds.height(), channels);
        const auto top = bounds.top() + lane * height;
        const auto bottom = lane + 1 >= count ? bounds.bottom() : top + height;
        return {bounds.left(), top, bounds.width(), bottom - top};
    }

    // Half height is the amplitude reference of the lane: full scale sits on the
    // lane edge, silence on the lane centre
    inline double laneHalfHeight(const QRectF &bounds, const int channels) {
        return laneHeight(bounds.height(), channels) * 0.5;
    }

    inline QString channelTag(const int channels, const int lane) {
        if (channels == 2)
            return lane == 0 ? QStringLiteral("L") : QStringLiteral("R");
        return QString::number(lane + 1);
    }
}

#endif // AUDIOLANEGEOMETRY_H
