//
// Created by OrangeCat on 24-9-4.
//

#include "AppModelUtils.h"

#include "Model/AppModel/Note.h"

void AppModelUtils::copyNotes(const QList<Note *> &source, QList<Note *> &target) {
    target.clear();
    for (const auto &note : source) {
        auto newNote = new Note;
        newNote->setClip(note->clip());
        newNote->setRStart(note->rStart());
        // newNote->setStart(note->start());
        newNote->setLength(note->length());
        newNote->setKeyIndex(note->keyIndex());
        newNote->setLyric(note->lyric());
        newNote->setPronunciation(note->pronunciation());
        newNote->setPronCandidates(note->pronCandidates());
        newNote->setPhonemes(note->phonemes());
        newNote->setLanguage(note->language());
        newNote->setLineFeed(note->lineFeed());
        target.append(newNote);
    }
}