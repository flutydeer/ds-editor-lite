//
// Created by OrangeCat on 24-9-4.
//

#ifndef APPMODELUTILS_H
#define APPMODELUTILS_H

#include <QList>

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
    DrawCurve getResultCurve(const DrawCurve &original, const DrawCurveList &edited);
};

#endif // APPMODELUTILS_H
