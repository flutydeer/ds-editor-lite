//
// Created by fluty on 24-10-7.
//

#include "InferPiece.h"

#include "AppModel.h"
#include "SingingClip.h"
#include "Note.h"

InferPiece::InferPiece(SingingClip *clip) : QObject(clip), clip(clip) {
    acousticInferStatus.onChanged(qSignalCallback(statusChanged));
}

int InferPiece::clipId() const {
    return clip->id();
}

int InferPiece::noteStartTick() const {
    return notes.first()->localStart();
}

int InferPiece::noteEndTick() const {
    return notes.last()->localStart() + notes.last()->length();
}

int InferPiece::localStartTick() const {
    auto firstNote = notes.first();
    auto phoneInfo = firstNote->phonemeOffsetInfo();
    auto aheadInfo = phoneInfo.ahead.result();
    auto normalInfo = phoneInfo.normal.result();
    auto phoneOffsets = aheadInfo.isEmpty() ? normalInfo : aheadInfo;
    auto firstOffset = phoneOffsets.isEmpty() ? 0 : phoneOffsets.first();
    int paddingTicks = appModel->msToTick(150 + firstOffset); // SP 0.15s
    return noteStartTick() - paddingTicks;
}

int InferPiece::localEndTick() const {
    int paddingTicks = appModel->msToTick(150); // SP 0.1s
    return noteEndTick() + paddingTicks;
}

const DrawCurve *InferPiece::getOriginalCurve(ParamInfo::Name name) const {
    switch (name) {
        case ParamInfo::Pitch:
            return &originalPitch;
        case ParamInfo::Breathiness:
            return &originalBreathiness;
        case ParamInfo::Tension:
            return &originalTension;
        case ParamInfo::Voicing:
            return &originalVoicing;
        case ParamInfo::Energy:
            return &originalEnergy;
        case ParamInfo::MouthOpening:
            return &originalMouthOpening;
        default:
            qFatal() << "Param type out of range" << name;
            return nullptr;
    }
}

void InferPiece::setOriginalCurve(ParamInfo::Name name, DrawCurve &curve) {
    switch (name) {
        case ParamInfo::Pitch:
            originalPitch = curve;
            break;
        case ParamInfo::Breathiness:
            originalBreathiness = curve;
            break;
        case ParamInfo::Tension:
            originalTension = curve;
            break;
        case ParamInfo::Voicing:
            originalVoicing = curve;
            break;
        case ParamInfo::Energy:
            originalEnergy = curve;
            break;
        case ParamInfo::MouthOpening:
            originalMouthOpening = curve;
            break;
        default:
            qFatal() << "Param type out of range" << name;
    }
}

const DrawCurve *InferPiece::getInputCurve(ParamInfo::Name name) const {
    switch (name) {
        case ParamInfo::Expressiveness:
            return &inputExpressiveness;
        case ParamInfo::Pitch:
            return &inputPitch;
        case ParamInfo::Breathiness:
            return &inputBreathiness;
        case ParamInfo::Tension:
            return &inputTension;
        case ParamInfo::Voicing:
            return &inputVoicing;
        case ParamInfo::Energy:
            return &inputEnergy;
        case ParamInfo::MouthOpening:
            return &inputMouthOpening;
        case ParamInfo::Gender:
            return &inputGender;
        case ParamInfo::Velocity:
            return &inputVelocity;
        case ParamInfo::ToneShift:
            return &inputToneShift;
        default:
            qFatal() << "Param type out of range" << name;
            return nullptr;
    }
}

void InferPiece::setInputCurve(ParamInfo::Name name, DrawCurve &curve) {
    switch (name) {
        case ParamInfo::Expressiveness:
            inputExpressiveness = curve;
        case ParamInfo::Pitch:
            inputPitch = curve;
            break;
        case ParamInfo::Breathiness:
            inputBreathiness = curve;
            break;
        case ParamInfo::Tension:
            inputTension = curve;
            break;
        case ParamInfo::Voicing:
            inputVoicing = curve;
            break;
        case ParamInfo::Energy:
            inputEnergy = curve;
            break;
        case ParamInfo::MouthOpening:
            inputMouthOpening = curve;
            break;
        case ParamInfo::Gender:
            inputGender = curve;
            break;
        case ParamInfo::Velocity:
            inputVelocity = curve;
            break;
        case ParamInfo::ToneShift:
            inputToneShift = curve;
            break;
        default:
            qFatal() << "Param type out of range" << name;
    }
}