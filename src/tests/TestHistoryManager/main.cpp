//
// Created by fluty on 2024/2/1.
//


#include "TestNoteActions.h"


#include <QDebug>

#include "../../gui/Controller/History/HistoryManager.h"
#include "Note.h"


int main(int argc, char *argv[]) {
    auto historyMgr = HistoryManager::instance();

    auto singingClip = new SingingClip;

    auto note1 = new Note;
    note1->start =480;
    note1->lyric="miao";
    QList<Note *> noteList1;
    noteList1.append(note1);
    auto actionSeq1 = new TestNoteActions;
    actionSeq1->addNotes(noteList1, singingClip);
    actionSeq1->execute();
    historyMgr->record(actionSeq1);
    qDebug() << "Add note:" << note1->start << note1->length << note1->lyric;

    qDebug() << "Clip model:";
    for (auto note : singingClip->notes)
        qDebug() << note->lyric;

    auto note2 = new Note;
    note2->start = 960;
    note2->lyric ="miao";
    QList<Note *> noteList2;
    noteList2.append(note1);
    auto actionSeq2 = new TestNoteActions;
    actionSeq2->addNotes(noteList2, singingClip);
    actionSeq2->execute();
    historyMgr->record(actionSeq2);
    qDebug() << "Add note:" << note2->start << note2->length << note2->lyric;

    qDebug() << "Clip model:";
    for (auto note : singingClip->notes)
        qDebug() << note->lyric;

    qDebug() << "Undo";
    historyMgr->undo();
    qDebug() << "Clip model:";
    for (auto note : singingClip->notes)
        qDebug() << note->lyric;

    qDebug() << "Redo";
    historyMgr->undo();
    qDebug() << "Clip model:";
    for (auto note : singingClip->notes)
        qDebug() << note->lyric;

    return 0;
}