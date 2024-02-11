//
// Created by fluty on 2024/2/10.
//

#include "ClipEditorViewController.h"
#include "TracksViewController.h"
#include "Actions/AppModel/Note/NoteActions.h"
#include "History/HistoryManager.h"

void ClipEditorViewController::setCurrentSingingClip(DsSingingClip *clip) {
    m_clip = clip;
}
void ClipEditorViewController::onClipPropertyChanged(const Clip::ClipCommonProperties &args) {
    TracksViewController::instance()->onClipPropertyChanged(args);
}
void ClipEditorViewController::onRemoveNotes(const QList<int> &notesId) {
    QList<Note *> notesToDelete;
    for (const auto id : notesId)
        notesToDelete.append(m_clip->findNoteById(id));

    auto a = new NoteActions;
    a->removeNotes(notesToDelete, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
}
void ClipEditorViewController::onEditNotesLyrics(const QList<int> &notesId) {
    QList<Note *> notesToEdit;
    for (const auto id : notesId)
        notesToEdit.append(m_clip->findNoteById(id));
    QList<Note::NoteWordProperties *> args;

    // TODO: call TestFillLyric
    // 新的歌词、拼音和音素要装进 DsNote::NoteWordProperties
    // 不要动 notesToEdit，否则撤销重做系统无法备份更改
    for (const auto note : notesToEdit) {
        auto properties = new Note::NoteWordProperties;
        properties->lyric = "喵";
        properties->pronunciation = "miao";
        args.append(properties);
    }

    auto a = new NoteActions;
    a->editNotesWordProperties(notesToEdit, args, m_clip);
    a->execute();
    HistoryManager::instance()->record(a);
}