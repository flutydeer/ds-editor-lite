#include "SingingClipPreviewLayout.h"

#include <algorithm>

namespace SingingClipPreview {
    double Layout::keyIndexAt(const double y) const {
        return highestKeyIndex - (y - contentTop) / noteHeight;
    }

    Layout computeLayout(const QRectF &previewRect, const QList<int> &keyIndices,
                         const double maxNoteHeight) {
        Layout layout;
        if (keyIndices.isEmpty() || previewRect.height() <= 0.0 || maxNoteHeight <= 0.0)
            return layout;

        const auto [lowest, highest] = std::minmax_element(keyIndices.cbegin(), keyIndices.cend());
        layout.lowestKeyIndex = *lowest;
        layout.highestKeyIndex = *highest;
        const auto rowCount = layout.highestKeyIndex - layout.lowestKeyIndex + 1;
        layout.noteHeight = std::min(maxNoteHeight, previewRect.height() / rowCount);
        const auto contentHeight = rowCount * layout.noteHeight;
        layout.contentTop =
            previewRect.top() + std::max(0.0, previewRect.height() - contentHeight) * 0.5;
        return layout;
    }
}
