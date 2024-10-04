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
    constexpr double paddingSpLen = 0.1; // s
    InferWord word;
    auto commitWord = [&] {
        result.append(word);
        word = InferWord();
    };


    // TODO：处理第一个音符为 AP 的情况
    auto processFirstNote = [&] {
        auto firstNote = notes.first();
        if (firstNote.isSlur)
            qFatal() << "分段第一个音符不能为转音";

        // 计算出第一个 word 的长度
        // Add head SP
        int i = 0;
        double firstPhoneStart = 0;
        if (useOffsetInfo) {
            if (!firstNote.aheadNames.isEmpty())
                firstPhoneStart = firstNote.aheadOffsets.at(i) / 1000.0;
        }
        auto firstWordLen = paddingSpLen + firstPhoneStart;

        InferPhoneme spPhoneme;
        spPhoneme.token = "SP";
        spPhoneme.language = "zh";
        word.phones.append(spPhoneme);
        for (const auto &phoneme : firstNote.aheadNames) {
            InferPhoneme inferPhoneme;
            inferPhoneme.token = phoneme;
            inferPhoneme.language = "zh";
            if (useOffsetInfo) {
                inferPhoneme.start = firstWordLen - firstNote.aheadOffsets.at(i) / 1000.0;
                i++;
            }
            word.phones.append(inferPhoneme);
        }

        InferNote spNote;
        spNote.is_rest = true;
        spNote.duration = firstWordLen;
        word.notes.append(spNote);
        commitWord();
    };
    processFirstNote();

    auto processNote = [&](const int &noteIndex, const InferDurPitNote &note) {
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

        // 处理当前音符后面还有音符的情况
        // TODO: 处理后方音符为 AP 的情况
        QList<InferPhoneme> stashedGapPhones; // 预留给间隙音符的音素列表
        int gapLen = 0;
        if (noteIndex < notes.size() - 1) {
            // 检查音符是否存在间隙
            gapLen = notes.at(noteIndex + 1).start - (note.start + note.length);
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
                if (gapLen == 0) // 如果没有间隙，则将下一个音符头部的音素装入当前 word 中
                    word.phones.append(inferPhoneme);
                else // 如果有间隙则暂存，留给间隙音符
                    stashedGapPhones.append(inferPhoneme);
            }
        }

        // 提交当前的音符
        bool commitFlag = true;
        if (noteIndex < notes.size() - 1)
            if (notes.at(noteIndex + 1).isSlur)
                commitFlag = false;
        if (noteIndex == notes.count() - 1)
            commitFlag = true;
        if (commitFlag)
            commitWord();

        // 如果存在间隙，则再提交一个填充间隙的音符
        if (gapLen > 0) {
            InferNote gapNote;
            gapNote.duration = tickToSec(gapLen);
            gapNote.is_rest = true;
            word.notes.append(gapNote);

            InferPhoneme gapPhoneme;
            gapPhoneme.token = "SP";
            gapPhoneme.language = "zh";
            word.phones.append(gapPhoneme);
            word.phones.append(stashedGapPhones);

            commitWord();
        }
    };

    int noteIndex = 0;
    for (auto &note : notes) {
        processNote(noteIndex, note);
        noteIndex++;
    }

    // Add tail SP
    InferNote spNote;
    spNote.duration = paddingSpLen;
    spNote.is_rest = true;
    InferPhoneme spPhoneme;
    spPhoneme.token = "SP";
    spPhoneme.language = "zh";
    word.notes.append(spNote);
    word.phones.append(spPhoneme);
    commitWord();

    return result;
}