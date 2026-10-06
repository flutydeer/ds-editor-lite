#ifndef DS_EDITOR_LITE_SLICERESULT_H
#define DS_EDITOR_LITE_SLICERESULT_H

#include <QList>
#include <QPair>
#include <QString>

class Note;
using NoteList = QList<Note *>;

enum class SliceExclusionReason {
    MissingPhonemes,
    FirstNoteInvalid,
    UnassignedSyllabification,
    Overlapped,
};

struct ExcludedNoteInfo {
    int noteId = -1;
    SliceExclusionReason reason = SliceExclusionReason::MissingPhonemes;
};

// Why a note is excluded from inference and thus stays silent. The detail is
// the failure message reported by the last phoneme task, may be empty
struct NoteInferenceErrorInfo {
    SliceExclusionReason reason = SliceExclusionReason::MissingPhonemes;
    QString detail;

    bool operator==(const NoteInferenceErrorInfo &) const noexcept = default;
};

class Segment {
public:
    double headAvailableLengthMs = 0;
    double paddingStartMs = 0;
    double paddingEndMs = 0;
    QList<Note *> notes;
};

class SliceResult {
public:
    QList<Segment> segments;
    // Notes that cause their phrase to be skipped or are dropped individually,
    // i.e. the root causes of silence, not the phrase-mates silenced along with them
    QList<ExcludedNoteInfo> excludedNotes;
    // Local tick ranges of the phrases skipped as a whole
    QList<QPair<int, int>> skippedPhraseRanges;
};

#endif //DS_EDITOR_LITE_SLICERESULT_H
