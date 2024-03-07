//
// Created by fluty on 24-3-7.
//

#include "NoteLayer.h"

#include <QList>

#include "../GraphicsItem/NoteGraphicsItem.h"

QList<NoteGraphicsItem *> NoteLayer::noteItems() const {
    QList<NoteGraphicsItem *> notes;
    for (auto item : items())
        notes.append(reinterpret_cast<NoteGraphicsItem *>(item));
    return notes;
}
NoteGraphicsItem *NoteLayer::findNoteById(int id) {
    for (const auto note : noteItems())
        if (note->id() == id)
            return note;
    return nullptr;
}
void NoteLayer::updateOverlappedState() {
    // for (const auto note : singingClip->notes()) {
    //     auto noteItem = m_noteLayer.findNoteById(note->id());
    //     noteItem->setOverlapped(note->overlapped());
    // }
}