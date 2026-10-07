#ifndef NOTEADJACENCYUTILS_H
#define NOTEADJACENCYUTILS_H

#include "UI/Views/Common/EditorResizeUtils.h"

#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <algorithm>

// Timeline neighbors of a note inside SingingClip's note list. The list is ordered
// by Note::compareTo (start, key, id), so the immediate predecessor/successor is
// the adjacent note in time regardless of pitch — the counterpart a Shift boundary
// drag moves together with the pressed note.
namespace NoteAdjacencyUtils {
    // Whether the pair shares a boundary: left ends exactly where right starts
    inline bool strictlyAdjacent(const Note *left, const Note *right) {
        return left && right && left->localStart() + left->length() == right->localStart();
    }

    inline Note *nextNote(const SingingClip *clip, const Note *note) {
        if (!clip || !note)
            return nullptr;
        const auto &notes = clip->notes();
        const auto it = std::lower_bound(
            notes.begin(), notes.end(), note,
            [](const Note *left, const Note *right) { return left->compareTo(right) < 0; });
        if (it == notes.end() || *it != note || std::next(it) == notes.end())
            return nullptr;
        return *std::next(it);
    }

    inline Note *prevNote(const SingingClip *clip, const Note *note) {
        if (!clip || !note)
            return nullptr;
        const auto &notes = clip->notes();
        const auto it = std::lower_bound(
            notes.begin(), notes.end(), note,
            [](const Note *left, const Note *right) { return left->compareTo(right) < 0; });
        if (it == notes.end() || *it != note || it == notes.begin())
            return nullptr;
        return *std::prev(it);
    }

    struct EdgeNeighbor {
        Note *neighbor = nullptr;
        bool exactlyAdjacent = false;
    };

    // The timeline neighbor on the dragged side plus whether the pair really shares
    // that boundary. neighbor == nullptr: there is no note on that side at all, the
    // plain resize path applies. neighbor != nullptr && !exactlyAdjacent: gap or
    // overlap, which makes the joint drag illegal.
    inline EdgeNeighbor neighborForEdge(const SingingClip *clip, const Note *note,
                                        const EditorResizeUtils::HorizontalEdge edge) {
        EdgeNeighbor result;
        if (!clip || !note)
            return result;
        if (edge == EditorResizeUtils::HorizontalEdge::Right) {
            result.neighbor = nextNote(clip, note);
            result.exactlyAdjacent = strictlyAdjacent(note, result.neighbor);
        } else if (edge == EditorResizeUtils::HorizontalEdge::Left) {
            result.neighbor = prevNote(clip, note);
            result.exactlyAdjacent = strictlyAdjacent(result.neighbor, note);
        }
        return result;
    }
}

#endif // NOTEADJACENCYUTILS_H
