//
// Created by fluty on 24-10-7.
//

#include "InferPiece.h"

#include "AppModel.h"
#include "SingingClip.h"
#include "Note.h"

InferPiece::InferPiece(SingingClip *clip) : QObject(clip), clip(clip) {
    acousticInferStatus.setNotify(qSignalCallback(statusChanged));
}

int InferPiece::clipId() const {
    return clip->id();
}

int InferPiece::noteStartTick() const {
    return notes.first()->rStart();
}

int InferPiece::noteEndTick() const {
    return notes.last()->rStart() + notes.last()->length();
}

int InferPiece::realStartTick() const {
    auto firstNote = notes.first();
    auto phoneInfo = firstNote->phonemeOffsetInfo();
    auto aheadInfo = phoneInfo.ahead.result();
    auto normalInfo = phoneInfo.normal.result();
    auto phoneOffsets = aheadInfo.isEmpty() ? normalInfo : aheadInfo;
    auto firstOffset = phoneOffsets.isEmpty() ? 0 : phoneOffsets.first();
    auto paddingTicks = appModel->msToTick(100 + firstOffset); // SP 0.1s
    return noteStartTick() - paddingTicks;
}

int InferPiece::realEndTick() const {
    auto paddingTicks = appModel->msToTick(100); // SP 0.1s
    return noteEndTick() + paddingTicks;
}

const DrawCurve *InferPiece::getCurve(ParamInfo::Name name) const {
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

void InferPiece::setCurve(ParamInfo::Name name, DrawCurve &curve) {
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