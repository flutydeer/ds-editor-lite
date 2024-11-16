//
// Created by OrangeCat on 24-9-4.
//

#include "AppModelUtils.h"

#include "MathUtils.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Curve.h"
#include "Model/AppModel/DrawCurve.h"
#include "Model/AppModel/Note.h"

void AppModelUtils::copyNotes(const QList<Note *> &source, QList<Note *> &target) {
    target.clear();
    for (const auto &note : source) {
        auto newNote = new Note;
        newNote->setClip(note->clip());
        newNote->setLocalStart(note->localStart());
        // newNote->setStart(note->start());
        newNote->setLength(note->length());
        newNote->setKeyIndex(note->keyIndex());
        newNote->setLyric(note->lyric());
        newNote->setPronunciation(note->pronunciation());
        newNote->setPronCandidates(note->pronCandidates());
        newNote->setPhonemes(note->phonemes());
        newNote->setLanguage(note->language());
        newNote->setLineFeed(note->lineFeed());
        target.append(newNote);
    }
}

QList<QList<Note *>> AppModelUtils::simpleSegment(const QList<Note *> &source, double threshold) {
    QList<QList<Note *>> target;
    if (source.isEmpty()) {
        qWarning() << "simpleSegment: source is empty";
        return {};
    }

    if (!target.isEmpty()) {
        target.clear();
        qWarning() << "simpleSegment: target is not empty, cleared";
    }

    QList<Note *> buffer;
    for (int i = 0; i < source.count(); i++) {
        auto note = source.at(i);
        buffer.append(note);
        bool commitFlag = false;
        if (i < source.count() - 1) {
            auto nextStartInMs = appModel->tickToMs(source.at(i + 1)->globalStart());
            auto curEndInMs = appModel->tickToMs((note->globalStart() + note->length()));
            commitFlag = nextStartInMs - curEndInMs > threshold;
        } else if (i == source.count() - 1)
            commitFlag = true;
        if (commitFlag) {
            target.append(buffer);
            buffer.clear();
        }
    }
    return target;
}

void AppModelUtils::copyCurves(const QList<Curve *> &source, QList<Curve *> &target) {
    target.clear();
    for (const auto curve : source) {
        if (curve->type() == Curve::Draw)
            target.append(new DrawCurve(*dynamic_cast<DrawCurve *>(curve)));
        // TODO: copy anchor curve
        // else if (curve->type() == Curve::Anchor)
        //     target.append(new AnchorCurve)
    }
}

void AppModelUtils::copyCurves(const QList<DrawCurve *> &source, QList<DrawCurve *> &target) {
    target.clear();
    for (const auto curve : source)
        target.append(new DrawCurve(*curve));
}

DrawCurveList AppModelUtils::curvesIn(const DrawCurveList &container, int startTick, int endTick) {
    DrawCurveList result;
    ProbeLine line(startTick, endTick);
    for (const auto &curve : container) {
        if (curve->isOverlappedWith(&line))
            result.append(curve);
    }
    return result;
}

DrawCurveList AppModelUtils::mergeCurves(const DrawCurveList &original,
                                         const DrawCurveList &edited) {
    DrawCurveList result;
    for (const auto &curve : original)
        result.append(new DrawCurve(*curve));

    for (const auto &editedCurve : edited) {
        auto newCurve = new DrawCurve(*editedCurve);
        auto overlappedOriCurves = curvesIn(result, editedCurve->localStart(), editedCurve->localEndTick());
        if (!overlappedOriCurves.isEmpty()) {
            for (auto oriCurve : overlappedOriCurves) {
                // 如果 oriCurve 被已编辑曲线完全覆盖，直接移除
                if (!(oriCurve->localStart() >= newCurve->localStart() &&
                      oriCurve->localEndTick() <= newCurve->localEndTick()))
                    newCurve->mergeWithCurrentPriority(*oriCurve);
                result.removeOne(oriCurve);
                delete oriCurve;
            }
        }
        MathUtils::binaryInsert(result, newCurve);
    }
    return result;
}

DrawCurve AppModelUtils::getResultCurve(const DrawCurve &original, const DrawCurveList &edited) {
    DrawCurve result = original;
    DrawCurveList curvesToMerge;
    for (const auto &curve : edited) {
        if (curve->isOverlappedWith(&result)) {
            auto newCurve = new DrawCurve(*curve);
            // 截断多余的部分
            if (curve->localStart() >= original.localStart() && curve->localEndTick() <= original.localEndTick()) {
                // original 曲线区间覆盖整条手绘曲线，无需截断
            } else
                newCurve->clip(original.localStart(), original.localEndTick());
            curvesToMerge.append(newCurve);
        }
    }
    for (const auto &curve : curvesToMerge) {
        result.mergeWithOtherPriority(*curve);
        delete curve;
    }
    return result;
}

DrawCurve AppModelUtils::getResultCurve(std::pair<int, int> tickRange, int baseValue,
                                        const DrawCurveList &edited) {
    auto startTick = MathUtils::round(tickRange.first, 5);
    auto endTick = MathUtils::round(tickRange.second,5);
    DrawCurve baseCurve;
    baseCurve.setLocalStart(startTick);
    for (int i = startTick; i < endTick; i += 5)
        baseCurve.appendValue(baseValue);

    DrawCurve result = getResultCurve(baseCurve, edited);
    return result;
}

DrawCurveList AppModelUtils::getDrawCurves(const QList<Curve *> &curves) {
    DrawCurveList result;
    for (const auto curve : curves)
        if (curve->type() == Curve::Draw)
            result.append(reinterpret_cast<DrawCurve *>(curve));
    return result;
}