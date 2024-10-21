//
// Created by OrangeCat on 24-9-4.
//

#ifndef APPMODELUTILS_H
#define APPMODELUTILS_H

#include <QList>
#include <utility>

class Curve;
class DrawCurve;
class Note;

using NoteList = QList<Note *>;
using DrawCurveList = QList<DrawCurve *>;

namespace AppModelUtils {
    void copyNotes(const NoteList &source, NoteList &target);
    QList<QList<Note *>> simpleSegment(const NoteList &source, double threshold = 800);

    void copyCurves(const QList<Curve *> &source, QList<Curve *> &target);
    void copyCurves(const QList<DrawCurve *> &source, QList<DrawCurve *> &target);
    DrawCurveList curvesIn(const DrawCurveList &container, int startTick, int endTick);
    DrawCurveList mergeCurves(const DrawCurveList &original, const DrawCurveList &edited);
    // 合并自动参数和手绘参数
    DrawCurve getResultCurve(const DrawCurve &original, const DrawCurveList &edited);
    // 合并仅有手绘的参数（如包络）
    DrawCurve getResultCurve(std::pair<int, int> tickRange, int baseValue,
                             const DrawCurveList &edited);
    DrawCurveList getDrawCurves(const QList<Curve *> &curves);
};

#endif // APPMODELUTILS_H
