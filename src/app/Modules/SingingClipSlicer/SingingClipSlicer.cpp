//
// Created by FlutyDeer on 2025/11/27.
//

#include "SingingClipSlicer.h"

#include "Global/SingingClipSlicerGlobal.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/Timeline.h"

#include <QDebug>

SliceResult SingingClipSlicer::slice(const Timeline &timeline, const NoteList &source) {
    // 切分选项
    auto headerLengthMax = SingingClipSlicerGlobal::headerAvailableLengthMax;
    auto padBaseLength = SingingClipSlicerGlobal::padBaseLength;
    auto padUnitAdditionalLength = SingingClipSlicerGlobal::padUnitAdditionalLength;

    if (source.isEmpty()) {
        qWarning() << "advancedSlice: source is empty";
        return {};
    }

    auto isRestNote = [](const Note &note) {
        const auto lyric = note.lyric().trimmed();
        return lyric == "AP" || lyric == "SP";
    };

    // 根据音符计算头部最小可用长度，分两种情况：
    //
    // 1. 非休止符（AP/SP），需补充 SP 音符
    // Note:  |          SP          |        Lyric           |
    // Phone: | SP | ph1 | ... | phn | (onset ph) |    ...    |
    // 最小值 = 基础量 + 头部音素数量（卡拍点前的所有音素数量） * 附加量基数
    //
    // 如果音符没有头部音素，则最小值退化为基础量
    // Note:  | SP |        Lyric           |
    // Phone: | SP | (onset ph) |    ...    |
    //
    // 2. 休止符（AP/SP），无需补充 SP
    // Note:  |      AP      |        Lyric           |
    // Phone: |      AP      | (onset ph) |    ...    |
    // 最小值 = 0
    //
    // TODO 重构音素时需要同步修改获取头部音素数量的方式
    auto getHeaderMinLength = [=](const Note &note) -> double {
        if (isRestNote(note))
            return 0.0;

        const auto headerPhonemeCount = note.phonemes().nameInfo.ahead.result().count();
        return padBaseLength + static_cast<double>(headerPhonemeCount) * padUnitAdditionalLength;
    };

    // 根据音符计算尾部填充长度，分两种情况：
    // 1. 非休止符（AP/SP），需补充 SP 音符
    // 填充长度 = 基础量
    // 2. 休止符（AP/SP），无需补充 SP
    // 尾部音素序列为空，填充长度为 0
    auto getTailLength = [=](const Note &note) -> double {
        if (isRestNote(note))
            return 0.0;
        return padBaseLength;
    };

    QList<Segment> segments;
    double lastTailEndInMs = 0;

    NoteList buffer;
    for (int i = 0; i < source.count(); i++) {
        const auto curNote = source.at(i);
        buffer.append(curNote);
        bool commitFlag = false;
        if (i < source.count() - 1) {
            // 下一个音符的头部起始时间 = 音符起始时间 - 音符头部最小可用长度
            const auto nextNote = source.at(i + 1);
            const auto nextStartInMs = timeline.tickToMs(nextNote->globalStart());
            const auto nextHeaderStartInMs = nextStartInMs - getHeaderMinLength(*nextNote);

            // 当前音符尾部的结束时间
            const auto curEndInMs = timeline.tickToMs(curNote->globalStart() + curNote->length());
            const auto curTailEndInMs = curEndInMs + getTailLength(*curNote);
            commitFlag = nextHeaderStartInMs > curTailEndInMs;
        } else if (i == source.count() - 1)
            commitFlag = true;
        if (commitFlag) {
            Segment segment;
            const auto firstNote = buffer.first();
            const auto firstStartInMs = timeline.tickToMs(firstNote->globalStart());

            // 计算头部可用长度
            // 如果当前片段和上一片段间距足够长，则长度取最大值，否则取实际可用长度
            const auto gap = firstStartInMs - lastTailEndInMs;
            segment.headAvailableLengthMs = gap > headerLengthMax ? headerLengthMax : gap;

            // 计算头部和尾部填充长度
            if (const auto headerMinLength = getHeaderMinLength(*firstNote); headerMinLength > 0) {
                const auto headStartInMs = firstStartInMs - headerMinLength;
                segment.paddingStartMs = firstStartInMs - headStartInMs;
            }

            const auto last = buffer.last();
            const auto lastEndInMs = timeline.tickToMs(last->globalStart() + last->length());
            const auto tailLength = getTailLength(*last);
            const auto tailEndInMs = lastEndInMs + tailLength;
            lastTailEndInMs = tailEndInMs;
            if (tailLength > 0)
                segment.paddingEndMs = tailEndInMs - lastEndInMs;

            segment.notes = buffer;

            segments.append(segment);
            buffer.clear();
        }
    }
    return {segments};
}

// SliceResult SingingClipSlicer::simpleSlice(const NoteList &source, double threshold) {
//     QList<NoteList> target;
//     if (source.isEmpty()) {
//         qWarning() << "simpleSlice: source is empty";
//         return {};
//     }
//
//     if (!target.isEmpty()) {
//         target.clear();
//         qWarning() << "simpleSlice: target is not empty, cleared";
//     }
//
//     NoteList buffer;
//     for (int i = 0; i < source.count(); i++) {
//         const auto note = source.at(i);
//         buffer.append(note);
//         bool commitFlag = false;
//         if (i < source.count() - 1) {
//             const auto nextStartInMs = appModel->tickToMs(source.at(i + 1)->globalStart());
//             const auto curEndInMs = appModel->tickToMs(note->globalStart() + note->length());
//             commitFlag = nextStartInMs - curEndInMs > threshold;
//         } else if (i == source.count() - 1)
//             commitFlag = true;
//         if (commitFlag) {
//             target.append(buffer);
//             buffer.clear();
//         }
//     }
//     return target;
// }