#include "SingingClipPreviewLayout.h"

#include <QHash>
#include <QSet>

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

    QVector<EditorPreview::Note> projectNotes(const QVector<EditorPreview::Note> &modelNotes,
                                              const QVector<EditorPreview::Note> &editedNotes,
                                              const QList<int> &erasedNoteIds) {
        QHash<int, EditorPreview::Note> edits;
        edits.reserve(editedNotes.size());
        for (const auto &note : editedNotes)
            edits.insert(note.id, note);
        const QSet<int> erased(erasedNoteIds.cbegin(), erasedNoteIds.cend());

        QVector<EditorPreview::Note> result;
        result.reserve(modelNotes.size() + editedNotes.size());
        QSet<int> modelIds;
        for (const auto &note : modelNotes) {
            modelIds.insert(note.id);
            if (erased.contains(note.id))
                continue;
            result.append(edits.value(note.id, note));
        }
        for (const auto &note : editedNotes) {
            if (!modelIds.contains(note.id) && !erased.contains(note.id))
                result.append(note);
        }
        std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
            if (left.rStart != right.rStart)
                return left.rStart < right.rStart;
            return left.id < right.id;
        });
        return result;
    }

    QVector<EditorPreview::Note> projectNotes(const QVector<EditorPreview::Note> &modelNotes,
                                              const bool applyEdits,
                                              const QVector<EditorPreview::Note> &editedNotes,
                                              const QList<int> &erasedNoteIds) {
        return applyEdits ? projectNotes(modelNotes, editedNotes, erasedNoteIds)
                          : projectNotes(modelNotes);
    }

    QList<int> keyIndices(const QVector<EditorPreview::Note> &notes) {
        QList<int> result;
        result.reserve(notes.size());
        for (const auto &note : notes)
            result.append(note.keyIndex);
        return result;
    }
}
