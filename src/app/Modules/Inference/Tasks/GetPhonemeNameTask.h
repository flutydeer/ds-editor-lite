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

#include <utility>

class GetPhonemeNameTask final : public Task {
    Q_OBJECT
public:
    explicit GetPhonemeNameTask(const SingingClip &clip, const QList<PhonemeNameInput> &inputs);
    int clipId() const;
    bool success() const;
    QList<Note *> notesRef;

    QList<PhonemeNameResult> result;

private:
    class Syllable {
    public:
        QList<PhonemeName> phonemes;
    };

    void runTask() override;
    void processNotes();
    QList<PhonemeNameResult> getPhonemeNames();
    std::pair<bool, int> checkTrailingPlus(const QString &lyric);
    const QList<Syllable> splitSyllables(const QList<PhonemeName> &phonemes);

    QString m_clipSingerId;
    SingerInfo m_clipSingerInfo;

    int m_clipId = -1;
    std::atomic<bool> m_success{false};
    QList<PhonemeNameInput> m_inputs;
    QString m_previewText;
};



#endif // GETPHONEMENAMETASK_H
