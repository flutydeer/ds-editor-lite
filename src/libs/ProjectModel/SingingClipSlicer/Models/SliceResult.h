#ifndef DS_EDITOR_LITE_SLICERESULT_H
#define DS_EDITOR_LITE_SLICERESULT_H

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

class Note;
using NoteList = QList<Note *>;

/// Why a note cannot take part in inference, or why its result cannot be
/// trusted. The slicer reports the first four, the model layer reports the
/// language and phoneme ones, so the enum describes inference issues in
/// general rather than slicer exclusions alone.
enum class NoteInferenceIssueReason {
    MissingPhonemes,
    FirstNoteInvalid,
    UnassignedSyllabification,
    Overlapped,
    /// The note declares a conversion language the singer cannot route.
    UnsupportedLanguage,
    /// The final phonemes of the note are not accepted by the current model.
    IncompatiblePhoneme,
};

struct ExcludedNoteInfo {
    int noteId = -1;
    NoteInferenceIssueReason reason = NoteInferenceIssueReason::MissingPhonemes;
};

/// Why a note is excluded from inference and thus stays silent, or why its
/// phonemes cannot be synthesized. `detail` is the failure message reported by
/// the last phoneme task and may be empty.
struct NoteInferenceErrorInfo {
    NoteInferenceIssueReason reason = NoteInferenceIssueReason::MissingPhonemes;
    QString detail;
    /// Secondary issues of the same note, kept inspectable while `reason`
    /// remains the summary the UI shows first.
    QList<NoteInferenceIssueReason> otherReasons;
    /// The declared conversion language the singer cannot route.
    QString language;
    /// Addressable conversion routes of the singer, in route order. Empty means
    /// the singer has no addressable route at all.
    QStringList availableLanguages;
    /// The note keeps user-edited phonemes, so the conflict does not remove them.
    bool usesEditedPhonemes = false;
    /// The phoneme name that the model does not accept.
    QString phoneme;
    /// The dictionary language of that phoneme.
    QString phonemeLanguage;

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
