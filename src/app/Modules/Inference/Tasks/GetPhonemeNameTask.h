//
// Created by OrangeCat on 24-9-3.
//

#ifndef GETPHONEMENAMETASK_H
#define GETPHONEMENAMETASK_H

#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"

#include "Modules/Inference/Models/PhonemeNameInput.h"
#include "Modules/Inference/Models/PhonemeNameResult.h"
#include "Modules/Task/Task.h"

class GetPhonemeNameTask final : public Task {
    Q_OBJECT
public:
    explicit GetPhonemeNameTask(const SingingClip &clip, const QList<PhonemeNameInput> &inputs);
    int clipId() const;
    bool success() const;
    QList<Note *> notesRef;

    QList<PhonemeNameResult> result;

private:
    void runTask() override;
    void processNotes();
    QList<PhonemeNameResult> getPhonemeNames(const QStringList &input);

    QString m_clipSingerId;
    QString m_clipG2pId;
    int m_clipId = -1;
    std::atomic<bool> m_success{false};
    QList<PhonemeNameInput> m_inputs;
    QString m_previewText;
};



#endif // GETPHONEMENAMETASK_H
