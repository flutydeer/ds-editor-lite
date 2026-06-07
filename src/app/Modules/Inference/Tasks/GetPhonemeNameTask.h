//
// Created by OrangeCat on 24-9-3.
//

#ifndef GETPHONEMENAMETASK_H
#define GETPHONEMENAMETASK_H

#include "Modules/Inference/Models/NoteInferenceSnapshot.h"
#include "Modules/Inference/Models/PhonemeNameResult.h"
#include "Modules/PackageManager/Models/SingerInfo.h"
#include "Modules/Task/Task.h"

#include <utility>

class GetPhonemeNameTask final : public Task {
    Q_OBJECT
public:
    explicit GetPhonemeNameTask(int clipId, quint64 clipRevision,
                                const QList<NoteInferenceSnapshot> &notes,
                                const SingerInfo &singerInfo, double tempo);
    int clipId() const;
    quint64 clipRevision() const;
    QList<int> noteIds() const;
    bool success() const;

    QList<PhonemeNameResult> result;

private:
    class Syllable {
    public:
        QList<PhonemeName> phonemes;
    };

    void runTask() override;
    void processNotes();
    QList<PhonemeNameResult> getPhonemeNames();
    void distributePhonemes();
    static bool isPlusNote(const QString &lyric);
    std::pair<bool, int> checkTrailingPlus(const QString &lyric);
    const QList<Syllable> splitSyllables(const QList<PhonemeName> &phonemes);

    SingerInfo m_clipSingerInfo;

    int m_clipId = -1;
    quint64 m_clipRevision = 0;
    std::atomic<bool> m_success{false};
    QList<NoteInferenceSnapshot> m_inputs;
    double m_tempo = 120.0;
    QString m_previewText;
};



#endif // GETPHONEMENAMETASK_H
