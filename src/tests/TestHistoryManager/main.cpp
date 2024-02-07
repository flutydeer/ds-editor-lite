//
// Created by fluty on 2024/2/1.
//


#include "TestNoteActions.h"


#include <QDebug>

#include "History/HistoryManager.h"
#include "Note.h"


int main(int argc, char *argv[]) {
    auto historyMgr = HistoryManager::instance();

    auto singingClip = new SingingClip;
    qDebug() << "Can undo:" << historyMgr->canUndo() << "Can redo" << historyMgr->canRedo();

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
    qDebug() << "Can undo:" << historyMgr->canUndo() << "Can redo" << historyMgr->canRedo();

    auto note2 = new Note;
    note2->start = 960;
    note2->lyric ="meow";
    QList<Note *> noteList2;
    noteList2.append(note2);
    auto actionSeq2 = new TestNoteActions;
    actionSeq2->addNotes(noteList2, singingClip);
    actionSeq2->execute();
    historyMgr->record(actionSeq2);
    qDebug() << "Add note:" << note2->start << note2->length << note2->lyric;

    qDebug() << "Clip model:";
    for (auto note : singingClip->notes)
        qDebug() << note->lyric;
    qDebug() << "Can undo:" << historyMgr->canUndo() << "Can redo" << historyMgr->canRedo();

    qDebug() << "Undo";
    historyMgr->undo();
    qDebug() << "Clip model:";
    for (auto note : singingClip->notes)
        qDebug() << note->lyric;
    qDebug() << "Can undo:" << historyMgr->canUndo() << "Can redo" << historyMgr->canRedo();

    qDebug() << "Redo";
    historyMgr->redo();
    qDebug() << "Clip model:";
    for (auto note : singingClip->notes)
        qDebug() << note->lyric;
    qDebug() << "Can undo:" << historyMgr->canUndo() << "Can redo" << historyMgr->canRedo();

    qDebug() << "Undo";
    historyMgr->undo();
    qDebug() << "Clip model:";
    for (auto note : singingClip->notes)
        qDebug() << note->lyric;
    qDebug() << "Can undo:" << historyMgr->canUndo() << "Can redo" << historyMgr->canRedo();

    auto note3 = new Note;
    note3->start = 960;
    note3->lyric ="nya";
    QList<Note *> noteList3;
    noteList3.append(note3);
    auto actionSeq3 = new TestNoteActions;
    actionSeq3->addNotes(noteList3, singingClip);
    actionSeq3->execute();
    historyMgr->record(actionSeq3);
    qDebug() << "Add note:" << note3->start << note3->length << note3->lyric;
    qDebug() << "Clip model:";
    for (auto note : singingClip->notes)
        qDebug() << note->lyric;
    qDebug() << "Can undo:" << historyMgr->canUndo() << "Can redo" << historyMgr->canRedo();

    return 0;
}