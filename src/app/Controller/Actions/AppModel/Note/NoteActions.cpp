//
// Created by fluty on 2024/2/8.
//

#include "NoteActions.h"

#include "EditNotePositionAction.h"
#include "EditNoteStartAndLengthAction.h"
#include "EditNotesLengthAction.h"
#include "EditNoteWordPropertiesAction.h"
#include "EditPhonemeOffsetAction.h"
#include "InsertNoteAction.h"
#include "RemoveNoteAction.h"

void NoteActions::insertNotes(const QList<Note *> &notes, SingingClip *clip) {
    setName(tr("Insert note(s)"));
    addAction(new InsertNoteAction(notes, clip));
}

void NoteActions::removeNotes(const QList<Note *> &notes, SingingClip *clip) {
    setName(tr("Remove note(s)"));
    addAction(new RemoveNoteAction(notes, clip));
}

void NoteActions::editNotesStartAndLength(const QList<Note *> &notes, const int delta,
                                          SingingClip *clip) {
    setName(tr("Edit note start and length"));
    addAction(new EditNoteStartAndLengthAction(notes, delta, clip));
}

void NoteActions::editNotesLength(const QList<Note *> &notes, const int delta, SingingClip *clip) {
    setName(tr("Edit note length"));
    addAction(new EditNotesLengthAction(notes, delta, clip));
}

void NoteActions::editNotePosition(const QList<Note *> &notes, const int deltaTick,
                                   const int deltaKey, SingingClip *clip) {
    setName(tr("Edit note position"));
    addAction(new EditNotePositionAction(notes, deltaTick, deltaKey, clip));
}

void NoteActions::editNotesWordProperties(const QList<Note *> &notes,
                                          const QList<Note::WordProperties> &args,
                                          SingingClip *clip) {
    setName(tr("Edit note word properties"));
    addAction(new EditNoteWordPropertiesAction(notes, args, clip));
}

void NoteActions::editNotePhonemeOffset(Note *note, const Phonemes::Type type,
                                        const QList<int> &offsets, SingingClip *clip) {
    setName(tr("Edite phoneme offset"));
    addAction(new EditPhonemeOffsetAction(note, type, offsets, clip));
}