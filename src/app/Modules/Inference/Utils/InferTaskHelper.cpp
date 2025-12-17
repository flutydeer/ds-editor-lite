//
// Created by fluty on 24-10-2.
//

#include "InferTaskHelper.h"

#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Models/InferInputBase.h"
#include "Modules/Inference/Models/InferInputNote.h"

QList<InferWord> InferTaskHelper::buildWords(const InferInputBase &input, bool useOffsetInfo) {
    const auto &notes = input.notes;
    const auto &timeline = input.timeline;
    QList<InferWord> result;
    QList<InferNote> noteBuffer;
    QList<InferPhoneme> phoneBuffer;
    auto commit = [&] {
        phoneBuffer[0].is_onset = true;
        result.append({phoneBuffer, noteBuffer});
        noteBuffer.clear();
        phoneBuffer.clear();
    };

    auto firstNote = notes.first();
    if (firstNote.isSlur)
        qFatal() << "分段第一个音符不能为转音";

    // 如果第一个音符不是休止符，则填充 SP 音符，长度为 paddingStartMs
    if (!firstNote.isRest) {
        auto firstWordLen = input.paddingStartMs / 1000.0;
        noteBuffer.append({0, 0, firstWordLen, true});
        phoneBuffer.append({"SP", firstNote.languageDictId, true, 0});

        for (int i = 0; i < firstNote.aheadNames.count(); i++) {
            auto name = firstNote.aheadNames.at(i);
            double start = 0;
            if (useOffsetInfo) {
                start = firstWordLen - firstNote.aheadOffsets.at(i) / 1000.0;
                i++;
            }
            phoneBuffer.append({name, firstNote.languageDictId, false, start});
        }
        commit();
    }

    int noteIndex = 0;
    int lastKey = 0;
    double wordStart = 0;
    double wordLen = 0;
    while (noteIndex < notes.count()) {
        const auto &note = notes.at(noteIndex);
        lastKey = note.key;
        wordStart = timeline.tickToSec(note.start);
        wordLen = timeline.tickToSec(note.start + note.length) - wordStart;
        noteBuffer.append({note.key, 0, timeline.tickToSec(note.length), note.isRest});

        for (int i = 0; i < note.normalNames.count(); i++) {
            auto name = note.normalNames.at(i);
            double start = 0;
            if (useOffsetInfo) {
                if (name == "SP" || name == "AP")
                    start = 0;
                else
                    start = note.normalOffsets.at(i) / 1000.0;
            }
            phoneBuffer.append({name, note.languageDictId, false, start});
        }

        // 处理当前音符之后还有音符的情况
        double gapLen = 0;
        bool hasGap = false;
        QList<InferPhoneme> stashedNextPhones;
        if (noteIndex < notes.size() - 1) {
            // 如果当前音符之后有转音，则一直往后查找，直到能计算出当前 word 的长度
            bool reachLast = false;
            while (notes.at(noteIndex + 1).isSlur) {
                const auto &nextNote = notes.at(noteIndex + 1);
                noteBuffer.append(
                    {nextNote.key, 0, timeline.tickToSec(nextNote.length), nextNote.isRest});
                wordLen += timeline.tickToSec(nextNote.start + nextNote.length) -
                           timeline.tickToSec(nextNote.start);
                noteIndex++;
                if (noteIndex == notes.size() - 1) { // 查找找到了最后一个音符
                    reachLast = true;
                    break;
                }
            }
            if (!reachLast) {
                // 找到下一个非转音音符
                const auto &nextNonSlurNote = notes.at(noteIndex + 1);
                auto nextNoteStartMs = timeline.tickToSec(nextNonSlurNote.start);
                gapLen = nextNoteStartMs - (wordStart + wordLen);
                hasGap = !qFuzzyCompare(gapLen, 0);

                // 如果没有间隙，则根据当前 word 的长度来计算偏移量
                if (hasGap) // 如果有间隙，则根据间隙长度计算偏移量
                    wordLen = gapLen;
                for (int i = 0; i < nextNonSlurNote.aheadNames.count(); i++) {
                    auto name = nextNonSlurNote.aheadNames.at(i);
                    double start = 0;
                    if (useOffsetInfo)
                        start = wordLen - nextNonSlurNote.aheadOffsets.at(i) / 1000.0;
                    InferPhoneme phone = {name, note.languageDictId, false, start};
                    if (!hasGap)
                        phoneBuffer.append(phone);
                    else // 如果有间隙则暂存，留给间隙音符
                        stashedNextPhones.append(phone);
                }
            }
        }
        commit();

        // 如果存在间隙，则再提交一个填充间隙的音符
        if (hasGap) {
            noteBuffer.append({lastKey, 0, gapLen, true});
            phoneBuffer.append({"SP", note.languageDictId, true, 0});
            phoneBuffer.append(stashedNextPhones);
            commit();
        }
        noteIndex++;
    }

    // 如果最后一个音符不为休止符，则填充一个尾部 SP 音符，长度为 paddingEndMs
    auto lastNote = notes.last();
    if (!lastNote.isRest) {
        noteBuffer.append({lastKey, 0, input.paddingEndMs / 1000.0, true});
        phoneBuffer.append({"SP", firstNote.languageDictId, true, 0});
        commit();
    }

    return result;
}