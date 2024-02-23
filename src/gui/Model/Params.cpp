//
// Created by fluty on 24-2-23.
//

#include "Params.h"
const OverlapableSerialList<Curve> &Param::curves(ParamType type) {
    switch (type) {
        case Original:
            return original;
        case Edited:
            return edited;
        case Envelope:
            return envelope;
        case Unknown:
        default:
            return unknown;
    }
}
void Param::setCurves(ParamType type, const OverlapableSerialList<Curve> &curves) {
    switch (type) {
        case Original:
            original = curves;
            break;
        case Edited:
            edited = curves;
            break;
        case Envelope:
            envelope = curves;
            break;
        case Unknown:
            break;
    }
}
Param *ParamBundle::getParamByName(ParamName name) {
    switch (name) {
        case Pitch:
            return &pitch;
        case Energy:
            return &energy;
        case Tension:
            return &tension;
        case Breathiness:
            return &breathiness;
        case Unknown:
        default:
            return nullptr;
    }
}