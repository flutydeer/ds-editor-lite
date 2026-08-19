#ifndef NOTEACTIONS_H
#define NOTEACTIONS_H

#include "EditNoteWordPropertiesAction.h"

#include <lite/History/ActionSequence.h>
#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/ProjectModel/AppModel/Note.h>

#include <QPair>

class SingingClip;
class Track;

class NoteActions : public ActionSequence {
    Q_OBJECT

public:
    void insertNotes(const QList<Note *> &notes, SingingClip *clip);
    void pasteNotes(const QList<Note *> &notes, SingingClip *clip, Track *track,
                    const Clip::ClipCommonProperties &newClipProperties);
    void removeNotes(const QList<Note *> &notes, SingingClip *clip);

    // Resize from left
    void editNotesStartAndLength(const QList<Note *> &notes, int delta, SingingClip *clip);

    // Resize from right
    void editNotesLength(const QList<Note *> &notes, int delta, SingingClip *clip);

    // Move notes
    void editNotePosition(const QList<Note *> &notes, int deltaTick, int deltaKey,
                          SingingClip *clip);

    // Edit lyrics, pronunciations and phonemes
    void editNotesWordProperties(const QList<Note *> &notes,
                                 const QList<Note::WordProperties> &args, SingingClip *clip,
                                 const QList<WordPropertyEditOptions> &options = {});
    void editNotePhonemeOffset(Note *note, const QList<int> &offsets, SingingClip *clip);

    // Split note
    void splitNote(Note *originalNote, Note *newNote, int newLength, SingingClip *clip);

    // Quantize: set absolute start/length values (per-note, not a uniform delta)
    void quantizeNotes(const QList<Note *> &notes,
                       const QList<QPair<int, int>> &newStartLengths, SingingClip *clip);
};



#endif // NOTEACTIONS_H
