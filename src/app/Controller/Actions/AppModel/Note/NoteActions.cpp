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
#include "SplitNoteAction.h"

#include <QCoreApplication>

void NoteActions::insertNotes(const QList<Note *> &notes, SingingClip *clip) {
    setTranslatableName("NoteActions", QT_TRANSLATE_NOOP("NoteActions", "Insert note(s)"));
    addAction(new InsertNoteAction(notes, clip));
}

void NoteActions::removeNotes(const QList<Note *> &notes, SingingClip *clip) {
    setTranslatableName("NoteActions", QT_TRANSLATE_NOOP("NoteActions", "Remove note(s)"));
    addAction(new RemoveNoteAction(notes, clip));
}

void NoteActions::editNotesStartAndLength(const QList<Note *> &notes, const int delta,
                                          SingingClip *clip) {
    setTranslatableName("NoteActions",
                        QT_TRANSLATE_NOOP("NoteActions", "Edit note start and length"));
    addAction(new EditNoteStartAndLengthAction(notes, delta, clip));
}

void NoteActions::editNotesLength(const QList<Note *> &notes, const int delta, SingingClip *clip) {
    setTranslatableName("NoteActions", QT_TRANSLATE_NOOP("NoteActions", "Edit note length"));
    addAction(new EditNotesLengthAction(notes, delta, clip));
}

void NoteActions::editNotePosition(const QList<Note *> &notes, const int deltaTick,
                                   const int deltaKey, SingingClip *clip) {
    setTranslatableName("NoteActions", QT_TRANSLATE_NOOP("NoteActions", "Edit note position"));
    addAction(new EditNotePositionAction(notes, deltaTick, deltaKey, clip));
}

void NoteActions::editNotesWordProperties(const QList<Note *> &notes,
                                          const QList<Note::WordProperties> &args,
                                          SingingClip *clip) {
    setTranslatableName("NoteActions",
                        QT_TRANSLATE_NOOP("NoteActions", "Edit note word properties"));
    addAction(new EditNoteWordPropertiesAction(notes, args, clip));
}

void NoteActions::editNotePhonemeOffset(Note *note, const QList<int> &offsets, SingingClip *clip) {
    setTranslatableName("NoteActions", QT_TRANSLATE_NOOP("NoteActions", "Edite phoneme offset"));
    addAction(new EditPhonemeOffsetAction(note, offsets, clip));
}

void NoteActions::splitNote(Note *originalNote, Note *newNote, int newLength, SingingClip *clip) {
    setTranslatableName("NoteActions", QT_TRANSLATE_NOOP("NoteActions", "Split note"));
    addAction(new SplitNoteAction(originalNote, newNote, newLength, clip));
}
