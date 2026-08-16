#ifndef SINGINGCLIPPREVIEWLAYOUT_H
#define SINGINGCLIPPREVIEWLAYOUT_H

#include <QList>
#include <QRectF>

namespace SingingClipPreview {
    inline constexpr double maximumNoteHeight = 8.0;

    struct Layout {
        int lowestKeyIndex = 127;
        int highestKeyIndex = 0;
        double noteHeight = 0.0;
        double contentTop = 0.0;

        [[nodiscard]] bool valid() const {
            return noteHeight > 0.0;
        }

        [[nodiscard]] double keyIndexAt(double y) const;
    };

    [[nodiscard]] Layout computeLayout(const QRectF &previewRect, const QList<int> &keyIndices,
                                       double maxNoteHeight = maximumNoteHeight);
}

#endif // SINGINGCLIPPREVIEWLAYOUT_H
