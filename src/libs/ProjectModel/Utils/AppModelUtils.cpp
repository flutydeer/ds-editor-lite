#include <lite/ProjectModel/Utils/AppModelUtils.h>

#include <lite/Support/MathUtils.h>
#include <lite/ProjectModel/SingingClipSlicer/SingingClipSlicerGlobal.h>
#include <lite/ProjectModel/AppModel/Curve.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/Note.h>

#include <algorithm>
#include <numeric>

namespace {
    DrawCurve *resampleCurve(const DrawCurve &curve, const int step) {
        if (curve.step == step)
            return new DrawCurve(curve);

        auto *result = new DrawCurve(curve);
        result->step = step;

        QList<int> values;
        values.reserve((curve.localEndTick() - curve.localStart()) / step);
        for (int tick = curve.localStart(); tick < curve.localEndTick(); tick += step) {
            const auto offset = tick - curve.localStart();
            const auto leftIndex = offset / curve.step;
            if (leftIndex >= curve.values().size() - 1) {
                values.append(curve.values().last());
                continue;
            }

            const auto remainder = offset % curve.step;
            if (remainder == 0) {
                values.append(curve.values().at(leftIndex));
                continue;
            }

            const auto leftValue = curve.values().at(leftIndex);
            const auto rightValue = curve.values().at(leftIndex + 1);
            values.append(qRound(leftValue +
                                 (rightValue - leftValue) * static_cast<double>(remainder) /
                                     curve.step));
        }
        result->setValues(values);
        return result;
    }
}

void AppModelUtils::copyNotes(const NoteList &source, NoteList &target) {
    target.clear();
    for (const auto &note : source) {
        const auto newNote = new Note;
        newNote->setClip(note->clip());
        newNote->setLocalStart(note->localStart());
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

void AppModelUtils::copyCurves(const QList<Curve *> &source, QList<Curve *> &target) {
    for (const auto curve : target)
        delete curve;
    target.clear();
    for (const auto curve : source) {
        if (curve->type() == Curve::Draw)
            target.append(new DrawCurve(*dynamic_cast<DrawCurve *>(curve)));
        else if (curve->type() == Curve::Anchor)
            target.append(new AnchorCurve(*dynamic_cast<AnchorCurve *>(curve)));
    }
}

void AppModelUtils::copyCurves(const QList<DrawCurve *> &source, QList<DrawCurve *> &target) {
    for (const auto curve : target)
        delete curve;
    target.clear();
    for (const auto curve : source)
        target.append(new DrawCurve(*curve));
}

DrawCurveList AppModelUtils::curvesIn(const DrawCurveList &container, const int startTick,
                                      const int endTick) {
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
        const auto newCurve = new DrawCurve(*editedCurve);
        auto overlappedOriCurves =
            curvesIn(result, editedCurve->localStart(), editedCurve->localEndTick());
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

bool AppModelUtils::overwriteDrawCurveRange(DrawCurveList &target, const DrawCurveList &source,
                                            int startTick, int endTick) {
    if (startTick > endTick)
        std::swap(startTick, endTick);
    if (startTick == endTick)
        return false;

    DrawCurveList replacements;
    for (const auto *curve : source) {
        if (!curve || curve->isEmpty() || curve->step <= 0)
            continue;

        const auto firstIndex =
            std::clamp((startTick - curve->localStart() + curve->step - 1) / curve->step, 0,
                       static_cast<int>(curve->values().size()));
        const auto lastIndex =
            std::clamp((endTick - curve->localStart() + curve->step - 1) / curve->step, 0,
                       static_cast<int>(curve->values().size()));
        if (firstIndex >= lastIndex)
            continue;

        auto *replacement = new DrawCurve;
        replacement->step = curve->step;
        replacement->setLocalStart(curve->localStart() + firstIndex * curve->step);
        replacement->setValues(curve->values().mid(firstIndex, lastIndex - firstIndex));
        replacements.append(replacement);
    }
    if (replacements.isEmpty())
        return false;

    while (!replacements.isEmpty()) {
        auto *replacement = replacements.takeFirst();
        const auto overlapped =
            curvesIn(target, replacement->localStart(), replacement->localEndTick());
        if (overlapped.isEmpty()) {
            MathUtils::binaryInsert(target, replacement);
            continue;
        }

        auto commonStep = replacement->step;
        for (const auto *curve : overlapped) {
            commonStep = std::gcd(commonStep, curve->step);
            commonStep = std::gcd(commonStep, curve->localStart() - replacement->localStart());
        }

        DrawCurveList normalizedTarget;
        for (const auto *curve : overlapped)
            normalizedTarget.append(resampleCurve(*curve, commonStep));
        DrawCurveList normalizedReplacement{resampleCurve(*replacement, commonStep)};
        auto merged = mergeCurves(normalizedTarget, normalizedReplacement);

        qDeleteAll(normalizedTarget);
        qDeleteAll(normalizedReplacement);
        delete replacement;
        for (auto *curve : overlapped) {
            target.removeOne(curve);
            delete curve;
        }
        for (auto *curve : merged)
            MathUtils::binaryInsert(target, curve);
    }
    return true;
}

DrawCurve AppModelUtils::getResultCurve(const DrawCurve &original, const DrawCurveList &edited) {
    DrawCurve result = original;
    DrawCurveList curvesToMerge;
    for (const auto &curve : edited) {
        if (curve->isOverlappedWith(&result)) {
            const auto newCurve = new DrawCurve(*curve);
            // 截断多余的部分
            if (curve->localStart() >= original.localStart() &&
                curve->localEndTick() <= original.localEndTick()) {
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

DrawCurve AppModelUtils::getResultCurve(const std::pair<int, int> tickRange, const int baseValue,
                                        const DrawCurveList &edited) {
    const auto startTick = MathUtils::round(tickRange.first, 5);
    const auto endTick = MathUtils::round(tickRange.second, 5);
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
            result.append(static_cast<DrawCurve *>(curve));
    return result;
}
