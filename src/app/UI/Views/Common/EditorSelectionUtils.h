#ifndef EDITORSELECTIONUTILS_H
#define EDITORSELECTIONUTILS_H

#include <QList>

namespace EditorSelectionUtils {
    inline QList<int> selectionForPress(const QList<int> &currentSelection, const int itemId,
                                        const bool toggle) {
        if (itemId < 0)
            return {};

        auto selection = currentSelection;
        if (toggle) {
            if (selection.contains(itemId))
                selection.removeAll(itemId);
            else
                selection.append(itemId);
        } else if (!selection.contains(itemId)) {
            selection = {itemId};
        }
        return selection;
    }
}

#endif // EDITORSELECTIONUTILS_H
