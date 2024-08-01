//
// Created by fluty on 24-3-7.
//

#include "NoteLayer.h"

#include <QList>

#include "../GraphicsItem/NoteView.h"

QList<NoteView *> NoteLayer::noteItems() const {
    QList<NoteView *> notes;
    for (auto item : items)
        notes.append(reinterpret_cast<NoteView *>(item));
    return notes;
}
NoteView *NoteLayer::findNoteById(int id) {
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