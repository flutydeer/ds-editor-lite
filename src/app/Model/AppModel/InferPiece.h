//
// Created by fluty on 24-9-15.
//

#ifndef INFERPIECE_H
#define INFERPIECE_H

#include "DrawCurve.h"
#include "InferStatus.h"
#include "Params.h"
#include "Interface/IInferPiece.h"
#include "Utils/Property.h"

#include <QObject>

class SingingClip;
class Note;

class InferPiece : public QObject, public IInferPiece {
    Q_OBJECT
public:
    Property<InferStatus> acousticInferStatus = Pending;
    SingingClip *clip;
    bool dirty = false;

    QString singerName;
    QList<Note *> notes;

    // Infer result
    DrawCurve pitch;
    DrawCurve breathiness;
    DrawCurve tension;
    DrawCurve voicing;
    DrawCurve energy;
    QString audioPath;

    explicit InferPiece(SingingClip *clip);

    [[nodiscard]] int clipId() const override;
    [[nodiscard]] int noteStartTick() const override;
    [[nodiscard]] int noteEndTick() const override;
    [[nodiscard]] int realStartTick() const;
    [[nodiscard]] int realEndTick() const;

    [[nodiscard]] const DrawCurve *getCurve(ParamInfo::Name name) const;
    void setCurve(ParamInfo::Name name, DrawCurve &curve);

signals:
    void statusChanged(InferStatus status);
};

#endif // INFERPIECE_H
