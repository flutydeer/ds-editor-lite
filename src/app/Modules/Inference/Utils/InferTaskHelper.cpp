#include "InferTaskHelper.h"

#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Models/InferInputBase.h"
#include "Modules/Inference/Models/InferInputNote.h"
#include <lite/ProjectModel/Utils/PhonemeHeadLayout.h>

#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logInferBuildWords, "infer.build_words")

QList<InferWord> InferTaskHelper::buildWords(const InferInputBase &input, bool useOffsetInfo) {
    const auto &notes = input.notes;
    const auto &timeline = input.timeline;
    if (notes.isEmpty()) {
        qCCritical(logInferBuildWords) << "buildWords: notes is empty - clipId:" << input.clipId
                                       << "pieceId:" << input.pieceId;
        return {};
    }

    auto hasNegativeValue = [](const double value) { return value < 0 && !qFuzzyIsNull(value); };
    const auto noteStartSeconds = [&](const InferInputNote &note) {
        return timeline.tickToSec(input.clipStartTick + note.start);
    };
    const auto noteLengthSeconds = [&](const InferInputNote &note) {
        const int globalStart = input.clipStartTick + note.start;
        return timeline.tickToSec(globalStart + note.length) - timeline.tickToSec(globalStart);
    };

    auto validateNonNegative = [&](const char *name, const double value, const int noteId) {
        if (hasNegativeValue(value))
            qCCritical(logInferBuildWords) << "buildWords:" << name << "is negative"
                                           << "noteId:" << noteId << "value:" << value;
    };

    if (useOffsetInfo) {
        for (const auto &note : notes) {
            if (!note.isRest && note.phonemeOffsets.count() != note.phonemeNames.count())
                qCCritical(logInferBuildWords)
                    << "buildWords: phoneme offset count does not match phoneme name count"
                    << "noteId:" << note.id << "offsetCount:" << note.phonemeOffsets.count()
                    << "nameCount:" << note.phonemeNames.count();
        }

        const auto headLayout = PhonemeHeadLayout::calculate(
            input.paddingStartMs, input.headAvailableLengthMs, notes.first().phonemeOffsets);
        if (!headLayout.isWithinBounds()) {
            qCCritical(logInferBuildWords)
                << "buildWords: first phoneme offsets exceed piece head boundary"
                << "clipId:" << input.clipId << "pieceId:" << input.pieceId
                << "noteId:" << notes.first().id
                << "minimumOffsetMs:" << headLayout.minimumFirstOffsetMs
                << "requiredHeadLengthMs:" << headLayout.requiredHeadLengthMs
                << "maximumHeadLengthMs:" << headLayout.maximumHeadLengthMs;
        }
        if (headLayout.minimumFirstOffsetMs != input.minimumFirstOffsetMs ||
            !qFuzzyCompare(headLayout.requiredHeadLengthMs + 1.0,
                           input.requiredHeadLengthMs + 1.0) ||
            !qFuzzyCompare(headLayout.maximumHeadLengthMs + 1.0, input.maximumHeadLengthMs + 1.0)) {
            qCCritical(logInferBuildWords)
                << "buildWords: phoneme head layout does not match the inference snapshot"
                << "clipId:" << input.clipId << "pieceId:" << input.pieceId
                << "noteId:" << notes.first().id;
        }
    }

    QList<InferWord> result;
    QList<InferNote> noteBuffer;
    QList<InferPhoneme> phoneBuffer;
    auto commit = [&] {
        if (phoneBuffer.isEmpty())
            return;
        phoneBuffer[0].is_onset = true;
        result.append({phoneBuffer, noteBuffer});
        noteBuffer.clear();
        phoneBuffer.clear();
    };

    auto firstNote = notes.first();
    if (firstNote.isSlur)
        qCCritical(logInferBuildWords) << "buildWords: first note of a segment cannot be a slur."
                                       << "clipId:" << input.clipId << "noteId:" << firstNote.id;
    if (firstNote.isPlus)
        qCCritical(logInferBuildWords)
            << "buildWords: first note of a segment cannot be an orphan plus note."
            << "clipId:" << input.clipId << "noteId:" << firstNote.id;

    // 如果第一个音符不是休止符，则填充 SP 音符
    if (!firstNote.isRest) {
        const auto firstWordLen =
            (useOffsetInfo ? input.requiredHeadLengthMs : input.paddingStartMs) / 1000.0;
        validateNonNegative("first word length", firstWordLen, firstNote.id);

        noteBuffer.append({0, 0, firstWordLen, true});
        phoneBuffer.append({"SP", firstNote.languageDictId, true, 0});

        for (int i = 0; i < firstNote.phonemeNames.count(); i++) {
            auto phonemeName = firstNote.phonemeNames.at(i);
            if (phonemeName.isOnset)
                break;

            double start = 0;
            if (useOffsetInfo) {
                start = firstWordLen + firstNote.phonemeOffsets.at(i) / 1000.0;
            }
            validateNonNegative("phoneme start", start, firstNote.id);
            phoneBuffer.append({phonemeName.name, phonemeName.language, false, start});
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
        wordStart = noteStartSeconds(note);
        wordLen = noteLengthSeconds(note);
        validateNonNegative("word length", wordLen, note.id);
        noteBuffer.append({note.key, 0, wordLen, note.isRest});

        bool foundOnset = false;
        for (int i = 0; i < note.phonemeNames.count(); i++) {
            auto phonemeName = note.phonemeNames.at(i);
            if (!phonemeName.isOnset && !foundOnset)
                continue;

            foundOnset = true;
            double start = 0;
            if (useOffsetInfo) {
                if (phonemeName.name == "SP" || phonemeName.name == "AP")
                    start = 0;
                else
                    start = note.phonemeOffsets.at(i) / 1000.0;
            }
            validateNonNegative("phoneme start", start, note.id);
            phoneBuffer.append({phonemeName.name, phonemeName.language, false, start});
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
                const double nextNoteLength = noteLengthSeconds(nextNote);
                noteBuffer.append({nextNote.key, 0, nextNoteLength, nextNote.isRest});
                wordLen += nextNoteLength;
                noteIndex++;
                if (noteIndex == notes.size() - 1) { // 查找找到了最后一个音符
                    reachLast = true;
                    break;
                }
            }
            if (!reachLast) {
                // 找到下一个非转音音符
                const auto &nextNonSlurNote = notes.at(noteIndex + 1);
                auto nextNoteStartMs = noteStartSeconds(nextNonSlurNote);
                gapLen = nextNoteStartMs - (wordStart + wordLen);
                validateNonNegative("gap length", gapLen, nextNonSlurNote.id);
                hasGap = !qFuzzyCompare(gapLen, 0);

                // 如果没有间隙，则根据当前 word 的长度来计算偏移量
                if (hasGap) // 如果有间隙，则根据间隙长度计算偏移量
                    wordLen = gapLen;
                for (int i = 0; i < nextNonSlurNote.phonemeNames.count(); i++) {
                    auto phonemeName = nextNonSlurNote.phonemeNames.at(i);
                    if (phonemeName.isOnset)
                        break;

                    double start = 0;
                    if (useOffsetInfo)
                        start = wordLen + nextNonSlurNote.phonemeOffsets.at(i) / 1000.0;
                    validateNonNegative("phoneme start", start, nextNonSlurNote.id);
                    InferPhoneme phone = {phonemeName.name, phonemeName.language, false, start};
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
            validateNonNegative("rest gap length", gapLen, note.id);
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
        validateNonNegative("tail padding length", input.paddingEndMs / 1000.0, lastNote.id);
        noteBuffer.append({lastKey, 0, input.paddingEndMs / 1000.0, true});
        phoneBuffer.append({"SP", firstNote.languageDictId, true, 0});
        commit();
    }

    return result;
}