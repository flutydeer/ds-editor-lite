//
// Created by fluty on 2024/2/7.
//

#ifndef TESTLISTACTIONS_H
#define TESTLISTACTIONS_H

#include "../../gui/Controller/History/ActionSequence.h"
#include "Note.h"
#include "SingingClip.h"

class TestNoteActions : public ActionSequence {
public:
    void addNotes(QList<Note *> notes, SingingClip *clip);
};



#endif // TESTLISTACTIONS_H
