//
// Created by fluty on 24-3-7.
//

#include "NoteLayer.h"

#include <QList>

#include "NoteView.h"
#include "Utils/MathUtils.h"

QList<NoteView *> NoteLayer::noteItems() const {
    QList<NoteView *> notes;
    for (auto item : items)
        notes.append(reinterpret_cast<NoteView *>(item));
    return notes;
}

NoteView *NoteLayer::findNoteById(int id) const {
    return MathUtils::findItemById<NoteView *>(noteItems(), id);
}