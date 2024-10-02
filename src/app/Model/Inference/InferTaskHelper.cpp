//
// Created by fluty on 24-10-2.
//

#include "InferTaskHelper.h"

#include "GenericInferModel.h"

QList<InferWord> InferTaskHelper::buildWords(const QList<InferDurPitNote> &notes, double tempo,
                                             bool useOffsetInfo) {
    auto tickToSec = [&](const double &tick) { return tick * 60 / tempo / 480; };

    QList<InferWord> result;
    double pos = 0;
    InferWord word;
    auto commitWord = [&] {
        result.append(word);
        word = InferWord();
    };

    constexpr double paddingSpLen = 0.1; // s
    InferNote spNote;
    spNote.duration = paddingSpLen;
    spNote.is_rest = true;
    InferPhoneme spPhoneme;
    spPhoneme.token = "SP";
    spPhoneme.language = "zh";

    auto processFirstNote = [&] {
        // Add head SP
        word.notes.append(spNote);
        word.phones.append(spPhoneme);

        auto firstNote = notes.first();
        if (firstNote.isSlur)
            qFatal() << "分段第一个音符不能为转音";

        int i = 0;
        for (const auto &phoneme : firstNote.aheadNames) {
            InferPhoneme inferPhoneme;
            inferPhoneme.token = phoneme;
            inferPhoneme.language = "zh";
            if (useOffsetInfo) {
                inferPhoneme.start = paddingSpLen - firstNote.aheadOffsets.at(i) / 1000.0;
                i++;
            }
            word.phones.append(inferPhoneme);
        }
        commitWord();
    };
    processFirstNote();

    int noteIndex = 0;
    for (auto &note : notes) {
        InferNote inferNote;
        inferNote.key = note.key;
        inferNote.duration = pos + tickToSec(note.length);
        word.notes.append(inferNote);

        int phoneIndex = 0;
        for (const auto &phoneme : note.normalNames) {
            InferPhoneme inferPhoneme;
            inferPhoneme.token = phoneme;
            inferPhoneme.language = "zh";
            if (useOffsetInfo) {
                inferPhoneme.start = note.normalOffsets.at(phoneIndex) / 1000.0;
                phoneIndex++;
            }
            word.phones.append(inferPhoneme);
        }

        if (noteIndex < notes.size() - 1) {
            phoneIndex = 0;
            for (const auto &phoneme : notes.at(noteIndex + 1).aheadNames) {
                InferPhoneme inferPhoneme;
                inferPhoneme.token = phoneme;
                inferPhoneme.language = "zh";
                if (useOffsetInfo) {
                    inferPhoneme.start =
                        inferNote.duration -
                        notes.at(noteIndex + 1).aheadOffsets.at(phoneIndex) / 1000.0;
                    phoneIndex++;
                }
                word.phones.append(inferPhoneme);
            }
        }

        bool commitFlag = true;
        if (noteIndex < notes.size() - 1)
            if (notes.at(noteIndex + 1).isSlur)
                commitFlag = false;

        if (noteIndex == notes.count() - 1)
            commitFlag = true;

        if (commitFlag)
            commitWord();
        noteIndex++;
    }

    // Add tail SP
    word.notes.append(spNote);
    word.phones.append(spPhoneme);
    commitWord();

    return result;
}