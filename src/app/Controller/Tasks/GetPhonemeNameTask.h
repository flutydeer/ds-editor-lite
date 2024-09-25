//
// Created by OrangeCat on 24-9-3.
//

#ifndef GETPHONEMENAMETASK_H
#define GETPHONEMENAMETASK_H

#include "Model/AppModel/Note.h"
#include "Model/Inference/PhonemeNameModel.h"
#include "Modules/Task/Task.h"

class GetPhonemeNameTask : public Task {
    Q_OBJECT
public:
    explicit GetPhonemeNameTask(int clipId, const QList<PhonemeNameInput> &inputs);
    // [[nodiscard]] QList<Note *> &notes();
    int clipId() const;
    QList<Note *> notesRef;

    QList<PhonemeNameResult> result;

private:
    void runTask() override;
    void processNotes();

    QMutex m_mutex;
    int m_clipId = -1;
    QList<PhonemeNameInput> m_inputs;
    QString m_previewText;
};



#endif // GETPHONEMENAMETASK_H
