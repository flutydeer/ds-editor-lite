//
// Created by fluty on 2024/1/27.
//

#ifndef DSPARAMS_H
#define DSPARAMS_H

#include "Curve.h"
#include "Utils/OverlapableSerialList.h"

class Param {
public:
    enum ParamType { Original, Edited, Envelope, Unknown };

    OverlapableSerialList<Curve> original;
    OverlapableSerialList<Curve> edited;
    OverlapableSerialList<Curve> envelope;

    const OverlapableSerialList<Curve> &curves(ParamType type);
    void setCurves(ParamType type, const OverlapableSerialList<Curve> &curves);

private:
    OverlapableSerialList<Curve> unknown;
};

class ParamBundle {
public:
    enum ParamName { Pitch, Energy, Tension, Breathiness, Unknown };
    Param pitch;
    Param energy;
    Param tension;
    Param breathiness;

    Param *getParamByName(ParamName name);

private:
    Param unknown;
};



#endif //DSPARAMS_H
