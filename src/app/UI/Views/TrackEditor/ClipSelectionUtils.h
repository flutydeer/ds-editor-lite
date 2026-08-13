#ifndef CLIPSELECTIONUTILS_H
#define CLIPSELECTIONUTILS_H

#include <QList>

namespace ClipSelectionUtils {
    inline QList<int> selectionForPress(const QList<int> &currentSelection, const int clipId,
                                        const bool toggle) {
        auto selection = currentSelection;
        if (toggle) {
            if (selection.contains(clipId))
                selection.removeAll(clipId);
            else
                selection.append(clipId);
        } else if (!selection.contains(clipId)) {
            selection = {clipId};
        }
        return selection;
    }
}

#endif // CLIPSELECTIONUTILS_H
