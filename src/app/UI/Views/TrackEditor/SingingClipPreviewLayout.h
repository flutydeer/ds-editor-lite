#ifndef SINGINGCLIPPREVIEWLAYOUT_H
#define SINGINGCLIPPREVIEWLAYOUT_H

#include "Model/AppStatus/EditorPreview.h"

#include <QList>
#include <QRectF>
#include <QVector>

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
    [[nodiscard]] QVector<EditorPreview::Note>
        projectNotes(const QVector<EditorPreview::Note> &modelNotes,
                     const QVector<EditorPreview::Note> &editedNotes = {},
                     const QList<int> &erasedNoteIds = {});
    [[nodiscard]] QVector<EditorPreview::Note>
        projectNotes(const QVector<EditorPreview::Note> &modelNotes, bool applyEdits,
                     const QVector<EditorPreview::Note> &editedNotes,
                     const QList<int> &erasedNoteIds);
    [[nodiscard]] QList<int> keyIndices(const QVector<EditorPreview::Note> &notes);
}

#endif // SINGINGCLIPPREVIEWLAYOUT_H
