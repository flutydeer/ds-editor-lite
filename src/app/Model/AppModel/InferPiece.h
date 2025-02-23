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

class InferPiece final : public QObject, public IInferPiece {
    Q_OBJECT
public:
    Property<InferStatus> acousticInferStatus = Pending;
    SingingClip *clip;
    bool dirty = false;

    QString configPath;
    QList<Note *> notes;

    // Infer result
    DrawCurve originalPitch;
    DrawCurve originalBreathiness;
    DrawCurve originalTension;
    DrawCurve originalVoicing;
    DrawCurve originalEnergy;
    QString audioPath;

    // Cached inputs
    DrawCurve inputExpressiveness;

    DrawCurve inputPitch;
    DrawCurve inputBreathiness;
    DrawCurve inputTension;
    DrawCurve inputVoicing;
    DrawCurve inputEnergy;

    DrawCurve inputGender;
    DrawCurve inputVelocity;
    DrawCurve inputToneShift;

    explicit InferPiece(SingingClip *clip);

    [[nodiscard]] int clipId() const override;
    [[nodiscard]] int noteStartTick() const override;
    [[nodiscard]] int noteEndTick() const override;
    [[nodiscard]] int localStartTick() const;
    [[nodiscard]] int localEndTick() const;

    [[nodiscard]] const DrawCurve *getOriginalCurve(ParamInfo::Name name) const;
    void setOriginalCurve(ParamInfo::Name name, DrawCurve &curve);

    [[nodiscard]] const DrawCurve *getInputCurve(ParamInfo::Name name) const;
    void setInputCurve(ParamInfo::Name name, DrawCurve &curve);

signals:
    void statusChanged(InferStatus status);
};

#endif // INFERPIECE_H
