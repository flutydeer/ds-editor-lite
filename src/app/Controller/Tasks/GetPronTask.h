//
// Created by OrangeCat on 24-9-3.
//

#ifndef GETPRONTASK_H
#define GETPRONTASK_H

#include "Model/AppModel/Note.h"
#include "Modules/Task/Task.h"

class GetPronTask : public Task {
    Q_OBJECT
public:
    explicit GetPronTask(int clipId, const QList<Note *> &notes);
    // [[nodiscard]] QList<Note *> &notes();
    QList<Note *> notesRef;

    QList<Note::NoteWordProperties> result;

private:
    void runTask() override;
    void processNotes();

    QMutex m_mutex;
    QList<Note *> m_notes;
    QString m_previewText;
};



#endif // GETPRONTASK_H
