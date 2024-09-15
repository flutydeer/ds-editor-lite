//
// Created by OrangeCat on 24-9-4.
//

#include "AppModelUtils.h"

#include "Model/AppModel/AppModel.h"
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

QList<QList<Note *>> AppModelUtils::simpleSegment(const QList<Note *> &source, double threshold) {
    QList<QList<Note *>> target;
    if (source.isEmpty()) {
        qWarning() << "simpleSegment: source is empty";
        return {};
    }

    if (!target.isEmpty()) {
        target.clear();
        qWarning() << "simpleSegment: target is not empty, cleared";
    }

    QList<Note *> buffer;
    for (int i = 0; i < source.count(); i++) {
        auto note = source.at(i);
        buffer.append(note);
        bool commitFlag = false;
        if (i < source.count() - 1) {
            auto nextStartInMs = appModel->tickToMs(source.at(i + 1)->start());
            auto curEndInMs = appModel->tickToMs((note->start() + note->length()));
            commitFlag = nextStartInMs - curEndInMs > threshold;
        } else if (i == source.count() - 1)
            commitFlag = true;
        if (commitFlag) {
            target.append(buffer);
            buffer.clear();
        }
    }
    return target;
}