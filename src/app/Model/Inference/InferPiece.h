//
// Created by fluty on 24-9-15.
//

#ifndef INFERPIECE_H
#define INFERPIECE_H

#include "Interface/IInferPiece.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/DrawCurve.h"
#include "Utils/Property.h"

#include <QList>

class Note;

class InferPiece : public QObject, public IInferPiece {
    Q_OBJECT
public:
    explicit InferPiece(SingingClip *clip) : QObject(clip), clip(clip) {
        acousticInferStatus.setNotify(qSignalCallback(statusChanged));
    };

    [[nodiscard]] int clipId() const override;

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

    [[nodiscard]] int noteStartTick() const override;
    [[nodiscard]] int noteEndTick() const override;
    [[nodiscard]] int realStartTick() const;
    [[nodiscard]] int realEndTick() const;

    [[nodiscard]] const DrawCurve *getCurve(ParamInfo::Name name) const;
    void setCurve(ParamInfo::Name name, DrawCurve &curve);

signals:
    void statusChanged(InferStatus status);
};

inline int InferPiece::clipId() const {
    return clip->id();
}

inline int InferPiece::noteStartTick() const {
    return notes.first()->rStart();
}

inline int InferPiece::noteEndTick() const {
    return notes.last()->rStart() + notes.last()->length();
}

// TODO: 范围需要重构
inline int InferPiece::realStartTick() const {
    auto firstNote = notes.first();
    auto phoneInfo = firstNote->phonemeOffsetInfo();
    auto aheadInfo = phoneInfo.ahead.result();
    auto normalInfo = phoneInfo.normal.result();
    auto phoneOffsets = aheadInfo.isEmpty() ? normalInfo : aheadInfo;
    auto firstOffset = phoneOffsets.isEmpty() ? 0 : phoneOffsets.first();
    auto paddingTicks = appModel->msToTick(100 + firstOffset); // SP 0.1s
    return noteStartTick() - paddingTicks;
    // return noteStartTick();
}

inline int InferPiece::realEndTick() const {
    auto paddingTicks = appModel->msToTick(100); // SP 0.1s
    return noteEndTick() + paddingTicks;
    // return noteEndTick();
}

inline const DrawCurve *InferPiece::getCurve(ParamInfo::Name name) const {
    switch (name) {
        case ParamInfo::Pitch:
            return &pitch;
        case ParamInfo::Breathiness:
            return &breathiness;
        case ParamInfo::Tension:
            return &tension;
        case ParamInfo::Voicing:
            return &voicing;
        case ParamInfo::Energy:
            return &energy;
        default:
            qFatal() << "Param type out of range" << name;
            return nullptr;
    }
}

inline void InferPiece::setCurve(ParamInfo::Name name, DrawCurve &curve) {
    switch (name) {
        case ParamInfo::Pitch:
            pitch = curve;
            break;
        case ParamInfo::Breathiness:
            breathiness = curve;
            break;
        case ParamInfo::Tension:
            tension = curve;
            break;
        case ParamInfo::Voicing:
            voicing = curve;
            break;
        case ParamInfo::Energy:
            energy = curve;
            break;
        default:
            qFatal() << "Param type out of range" << name;
    }
}

#endif // INFERPIECE_H
