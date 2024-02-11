//
// Created by fluty on 2024/2/8.
//

#ifndef NOTEACTIONS_H
#define NOTEACTIONS_H

#include "Controller/History/ActionSequence.h"
#include "Model/DsClip.h"
#include "Model/DsNote.h"

class NoteActions : public ActionSequence {
public:
    void insertNotes(const QList<DsNote *> &notes, DsSingingClip *clip);
    void removeNotes(const QList<DsNote *> &notes, DsSingingClip *clip);

    // Resize from left
    void editNotesStartAndLength(const QList<DsNote *> &notes, int delta, DsSingingClip *clip);

    // Resize from right
    void editNotesLength(const QList<DsNote *> &notes, int delta, DsSingingClip *clip);

    // Move notes
    void editNotePosition(const QList<DsNote *> &notes, int deltaTick, int deltaKey,
                          DsSingingClip *clip);

    // Edit lyrics, pronunciations and phonemes
    void editNotesWordProperties(const QList<DsNote *> &notes,
                                 const QList<DsNote::NoteWordProperties *> &args,
                                 DsSingingClip *clip);
};



#endif // NOTEACTIONS_H
