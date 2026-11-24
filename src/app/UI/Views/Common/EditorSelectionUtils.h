#ifndef EDITORSELECTIONUTILS_H
#define EDITORSELECTIONUTILS_H

#include <QList>
#include <Qt>

namespace EditorSelectionUtils {
    enum class SelectionMode { Plain, Toggle, ReplaceRange, AddRange };

    struct PressResult {
        QList<int> selection;
        bool targetSelected = false;
        // Ctrl pressing a selected item keeps it, so the press can start a drag
        // with it. The removal lands on release when the pointer never moved.
        bool deselectOnRelease = false;
    };

    inline QList<int> selectionForPress(const QList<int> &currentSelection, const int itemId,
                                        const bool toggle) {
        if (itemId < 0)
            return {};

        auto selection = currentSelection;
        // A selected item stays in the selection even for a toggle press: the
        // release removes it when the press never turned into a drag.
        if (selection.contains(itemId))
            return selection;
        if (toggle)
            selection.append(itemId);
        else
            selection = {itemId};
        return selection;
    }

    inline SelectionMode selectionModeForModifiers(const Qt::KeyboardModifiers modifiers) {
        const auto control = modifiers.testFlag(Qt::ControlModifier);
        if (modifiers.testFlag(Qt::ShiftModifier))
            return control ? SelectionMode::AddRange : SelectionMode::ReplaceRange;
        return control ? SelectionMode::Toggle : SelectionMode::Plain;
    }

    class OrderedSelectionModel {
    public:
        [[nodiscard]] int anchorId() const {
            return m_anchorId;
        }

        void clearAnchor() {
            m_anchorId = -1;
        }

        void cancelPress() {
            m_pressedItemId = -1;
            m_pressModifiers = Qt::NoModifier;
            m_pressActive = false;
            m_pressToggleOnRelease = false;
        }

        void invalidateAnchor(const int itemId) {
            if (m_anchorId == itemId)
                clearAnchor();
        }

        void synchronize(const QList<int> &selection) {
            if (selection.isEmpty()) {
                clearAnchor();
            } else if (!selection.contains(m_anchorId)) {
                m_anchorId = selection.constLast();
            }
        }

        void selectOnly(const int itemId) {
            m_anchorId = itemId;
        }

        PressResult press(const QList<int> &currentSelection, const QList<int> &orderedItems,
                          const int itemId, const Qt::KeyboardModifiers modifiers) {
            m_pressedItemId = itemId;
            m_pressModifiers = modifiers;
            m_pressActive = itemId >= 0;
            return applyPress(currentSelection, orderedItems, itemId,
                              selectionModeForModifiers(modifiers));
        }

        PressResult applyPress(const QList<int> &currentSelection, const QList<int> &orderedItems,
                               const int itemId, const SelectionMode mode) {
            m_pressToggleOnRelease = false;
            if (itemId < 0) {
                clearAnchor();
                return {};
            }

            PressResult result;
            if (mode == SelectionMode::Plain || mode == SelectionMode::Toggle) {
                m_anchorId = itemId;
                const auto toggle = mode == SelectionMode::Toggle;
                result.selection = selectionForPress(currentSelection, itemId, toggle);
                m_pressToggleOnRelease = toggle && currentSelection.contains(itemId);
            } else {
                const auto anchorIndex = orderedItems.indexOf(m_anchorId);
                const auto targetIndex = orderedItems.indexOf(itemId);
                if (anchorIndex < 0 || targetIndex < 0) {
                    m_anchorId = itemId;
                    result.selection = {itemId};
                } else {
                    result.selection =
                        mode == SelectionMode::AddRange ? currentSelection : QList<int>();
                    const auto first = qMin(anchorIndex, targetIndex);
                    const auto last = qMax(anchorIndex, targetIndex);
                    for (auto index = first; index <= last; ++index) {
                        const auto id = orderedItems.at(index);
                        if (!result.selection.contains(id))
                            result.selection.append(id);
                    }
                }
            }
            QList<int> orderedSelection;
            for (const auto id : orderedItems) {
                if (result.selection.contains(id))
                    orderedSelection.append(id);
            }
            result.selection = orderedSelection;
            result.targetSelected = result.selection.contains(itemId);
            result.deselectOnRelease = m_pressToggleOnRelease;
            return result;
        }

        [[nodiscard]] QList<int> release(const QList<int> &currentSelection,
                                         const bool pointerMoved) {
            auto selection = currentSelection;
            const auto preserveSelection = m_pressModifiers.testFlag(Qt::ControlModifier) ||
                                           m_pressModifiers.testFlag(Qt::ShiftModifier);
            if (m_pressToggleOnRelease && !pointerMoved) {
                // A Ctrl click that never moved completes the toggle the press
                // deferred. A drag keeps the item, because it already acted on it.
                selection.removeAll(m_pressedItemId);
            } else if (m_pressActive && !pointerMoved && !preserveSelection &&
                       currentSelection.contains(m_pressedItemId)) {
                selection = {m_pressedItemId};
            }
            cancelPress();
            return selection;
        }

    private:
        int m_anchorId = -1;
        int m_pressedItemId = -1;
        Qt::KeyboardModifiers m_pressModifiers = Qt::NoModifier;
        bool m_pressActive = false;
        bool m_pressToggleOnRelease = false;
    };
}

#endif // EDITORSELECTIONUTILS_H
