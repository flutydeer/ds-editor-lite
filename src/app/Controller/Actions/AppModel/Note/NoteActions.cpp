//
// Created by fluty on 2024/2/8.
//

#include "NoteActions.h"

#include "EditNotePositionAction.h"
#include "EditNoteStartAndLengthAction.h"
#include "EditNotesLengthAction.h"
#include "EditNotesWordPropertiesAction.h"
#include "EditPhonemeAction.h"
#include "InsertNoteAction.h"
#include "RemoveNoteAction.h"

void NoteActions::insertNotes(const QList<Note *> &notes, SingingClip *clip) {
    setName(tr("Insert note(s)"));
    for (const auto note : notes)
        addAction(InsertNoteAction::build(note, clip));
}
void NoteActions::removeNotes(const QList<Note *> &notes, SingingClip *clip) {
    setName(tr("Remove note(s)"));
    for (const auto note : notes)
        addAction(RemoveNoteAction::build(note, clip));
}
void NoteActions::editNotesStartAndLength(const QList<Note *> &notes, int delta,
                                          SingingClip *clip) {
    setName(tr("Edit note start and length"));
    for (const auto note : notes)
        addAction(EditNoteStartAndLengthAction::build(note, delta, clip));
}
void NoteActions::editNotesLength(const QList<Note *> &notes, int delta, SingingClip *clip) {
    setName(tr("Edit note length"));
    for (const auto note : notes)
        addAction(EditNotesLengthAction::build(note, delta, clip));
}
void NoteActions::editNotePosition(const QList<Note *> &notes, int deltaTick, int deltaKey,
                                   SingingClip *clip) {
    setName(tr("Edit note position"));
    for (const auto note : notes)
        addAction(EditNotePositionAction::build(note, deltaTick, deltaKey, clip));
}
void NoteActions::editNotesWordProperties(const QList<Note *> &notes,
                                          const QList<Note::NoteWordProperties *> &args,
                                          SingingClip *clip) {
    setName(tr("Edit note word properties"));
    int i = 0;
    for (const auto note : notes) {
        addAction(EditNotesWordPropertiesAction::build(note, args[i], clip));
        i++;
    }
}
void NoteActions::editNotesPhoneme(const QList<Note *> &notes, const QList<Phoneme> &phonemes,
                                   SingingClip *clip) {
    setName(tr("Edit note phoneme"));
    int i = 0;
    for (const auto note : notes) {
        addAction(EditPhonemeAction::build(note, phonemes[i], clip));
        i++;
    }
}
// TODO::editNotesLanguage