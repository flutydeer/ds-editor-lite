#include "NoteInferenceSnapshot.h"

#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

QList<NoteInferenceSnapshot> buildNoteInferenceSnapshots(const SingingClip &clip) {
    QList<NoteInferenceSnapshot> result;
    result.reserve(clip.notes().count());
    for (const auto note : clip.notes()) {
        NoteInferenceSnapshot snapshot;
        snapshot.noteId = note->id();
        snapshot.lyric = note->lyric();
        snapshot.language = note->effectiveLanguage();
        snapshot.pronunciation = note->pronunciation().result();
        snapshot.globalStart = note->globalStart();
        snapshot.length = note->length();
        snapshot.keyIndex = note->keyIndex();
        result.append(snapshot);
    }
    return result;
}
