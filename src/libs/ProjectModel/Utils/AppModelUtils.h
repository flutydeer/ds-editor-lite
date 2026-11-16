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

    void copyCurves(const QList<Curve *> &source, QList<Curve *> &target);
    void copyCurves(const QList<DrawCurve *> &source, QList<DrawCurve *> &target);
    DrawCurveList curvesIn(const DrawCurveList &container, int startTick, int endTick);
    DrawCurveList mergeCurves(const DrawCurveList &original, const DrawCurveList &edited);
    bool eraseDrawCurveRange(DrawCurveList &target, int startTick, int endTick);
    // Overlay hand-drawn edits on the original curve.
    DrawCurve getResultCurve(const DrawCurve &original, const DrawCurveList &edited);
    // Overlay hand-drawn edits on a constant baseline, as for envelope parameters.
    DrawCurve getResultCurve(std::pair<int, int> tickRange, int baseValue,
                             const DrawCurveList &edited);
    DrawCurveList getDrawCurves(const QList<Curve *> &curves);
};

#endif // APPMODELUTILS_H
