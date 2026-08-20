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
    int curveValueAt(const DrawCurve &curve, const int tick) {
        const auto offset = tick - curve.localStart();
        const auto leftIndex = offset / curve.step;
        if (leftIndex >= curve.values().size() - 1)
            return curve.values().last();

        const auto remainder = offset % curve.step;
        if (remainder == 0)
            return curve.values().at(leftIndex);

        const auto leftValue = curve.values().at(leftIndex);
        const auto rightValue = curve.values().at(leftIndex + 1);
        return qRound(leftValue +
                      (rightValue - leftValue) * static_cast<double>(remainder) / curve.step);
    }

    DrawCurve *resampleCurve(const DrawCurve &curve, const int startTick, const int endTick,
                             const int step) {
        if (curve.localStart() == startTick && curve.localEndTick() == endTick &&
            curve.step == step)
            return new DrawCurve(curve);

        auto *result = new DrawCurve(curve);
        result->step = step;
        result->setLocalStart(startTick);

        QList<int> values;
        values.reserve((endTick - startTick) / step);
        for (int tick = startTick; tick < endTick; tick += step)
            values.append(curveValueAt(curve, tick));
        result->setValues(values);
        return result;
    }

    DrawCurve *resampleCurve(const DrawCurve &curve, const int step) {
        return resampleCurve(curve, curve.localStart(), curve.localEndTick(), step);
    }

    void mergeAdjacentDrawCurves(DrawCurveList &curves) {
        for (qsizetype i = 0; i + 1 < curves.size();) {
            auto *left = curves.at(i);
            auto *right = curves.at(i + 1);
            if (!left || !right || left->isEmpty() || right->isEmpty() || left->step <= 0 ||
                right->step <= 0 || left->localEndTick() != right->localStart()) {
                ++i;
                continue;
            }

            const auto commonStep = std::gcd(left->step, right->step);
            auto *merged = resampleCurve(*left, commonStep);
            const auto *normalizedRight = resampleCurve(*right, commonStep);
            merged->insertValues(merged->values().size(), normalizedRight->values());

            curves[i] = merged;
            curves.removeAt(i + 1);
            delete left;
            delete right;
            delete normalizedRight;
        }
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

bool AppModelUtils::eraseDrawCurveRange(DrawCurveList &target, int startTick, int endTick) {
    if (startTick > endTick)
        std::swap(startTick, endTick);
    if (startTick == endTick)
        return false;

    const auto overlapped = curvesIn(target, startTick, endTick);
    for (auto *curve : overlapped) {
        if (!curve || curve->isEmpty() || curve->step <= 0)
            continue;

        const auto curveStart = curve->localStart();
        const auto curveEnd = curve->localEndTick();
        auto commonStep = curve->step;
        if (startTick > curveStart && startTick < curveEnd)
            commonStep = std::gcd(commonStep, startTick - curveStart);
        if (endTick > curveStart && endTick < curveEnd)
            commonStep = std::gcd(commonStep, endTick - curveStart);
        if (commonStep != curve->step) {
            auto *normalized = resampleCurve(*curve, commonStep);
            target[target.indexOf(curve)] = normalized;
            delete curve;
            curve = normalized;
        }

        if (curve->localStart() >= startTick && curve->localEndTick() <= endTick) {
            target.removeOne(curve);
            delete curve;
        } else if (curve->localStart() < startTick && curve->localEndTick() > endTick) {
            auto *rightCurve = new DrawCurve;
            rightCurve->step = curve->step;
            rightCurve->setClip(curve->Curve::clip());
            rightCurve->setLocalStart(endTick);
            rightCurve->setValues(curve->mid(endTick));
            curve->eraseTailFrom(startTick);
            MathUtils::binaryInsert(target, rightCurve);
        } else {
            curve->erase(startTick, endTick);
        }
    }
    return !overlapped.isEmpty();
}

bool AppModelUtils::bakeDrawCurveRange(DrawCurveList &target, const DrawCurveList &source,
                                       int startTick, int endTick) {
    if (startTick > endTick)
        std::swap(startTick, endTick);
    if (startTick == endTick)
        return false;

    DrawCurveList replacements;
    for (const auto *curve : source) {
        if (!curve || curve->isEmpty() || curve->step <= 0)
            continue;

        const auto replacementStart = std::max(startTick, curve->localStart());
        const auto replacementEnd = std::min(endTick, curve->localEndTick());
        if (replacementStart >= replacementEnd)
            continue;

        auto replacementStep = std::gcd(curve->step, replacementStart - curve->localStart());
        replacementStep = std::gcd(replacementStep, replacementEnd - replacementStart);
        replacements.append(
            resampleCurve(*curve, replacementStart, replacementEnd, replacementStep));
    }
    if (replacements.isEmpty())
        return false;

    for (auto *replacement : replacements) {
        eraseDrawCurveRange(target, replacement->localStart(), replacement->localEndTick());
        MathUtils::binaryInsert(target, replacement);
    }
    mergeAdjacentDrawCurves(target);
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
