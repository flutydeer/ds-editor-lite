//
// Created by fluty on 2024/2/8.
//

#include "NoteActions.h"

#include "EditNotePositionAction.h"
#include "EditNoteStartAndLengthAction.h"
#include "EditNotesLengthAction.h"
#include "EditNotesWordPropertiesAction.h"
#include "InsertNoteAction.h"
#include "RemoveNoteAction.h"

void NoteActions::insertNotes(const QList<DsNote *> &notes, DsSingingClip *clip) {
    for (const auto note : notes)
        addAction(InsertNoteAction::build(note, clip));
}
void NoteActions::removeNotes(const QList<DsNote *> &notes, DsSingingClip *clip) {
    for (const auto note : notes)
        addAction(RemoveNoteAction::build(note, clip));
}
void NoteActions::editNotesStartAndLength(const QList<DsNote *> &notes, int delta,
                                          DsSingingClip *clip) {
    for (const auto note : notes)
        addAction(EditNoteStartAndLengthAction::build(note, delta, clip));
}
void NoteActions::editNotesLength(const QList<DsNote *> &notes, int delta, DsSingingClip *clip) {
    for (const auto note : notes)
        addAction(EditNotesLengthAction::build(note, delta, clip));
}
void NoteActions::editNotePosition(const QList<DsNote *> &notes, int deltaTick, int deltaKey,
                                   DsSingingClip *clip) {
    for (const auto note : notes)
        addAction(EditNotePositionAction::build(note, deltaTick, deltaKey, clip));
}
void NoteActions::editNotesWordProperties(const QList<DsNote *> &notes,
                                          const QList<DsNote::NoteWordProperties *> &args,
                                          DsSingingClip *clip) {
    int i = 0;
    for (const auto note : notes) {
        addAction(EditNotesWordPropertiesAction::build(note, args[i], clip));
        i++;
    }
}