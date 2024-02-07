//
// Created by fluty on 2024/2/7.
//

#include "TestNoteActions.h"

#include "InsertAction.h"
void TestNoteActions::addNotes(QList<Note *> notes, SingingClip *clip) {
    for (auto note : notes) {
        auto action = InsertAction::build(note, clip);
        m_actionSequence.append(action);
    }
}
// void TestNoteActions::removeNotes(QList<DsNote *> notes, DsSingingClip *clip) {
//
// }