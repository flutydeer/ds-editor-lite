//
// Created by fluty on 2024/1/27.
//

#ifndef DSPARAMS_H
#define DSPARAMS_H

#include "Curve.h"
#include "Utils/OverlapableSerialList.h"

class Param {
public:
    enum ParamCurveType { Original, Edited, Envelope };

    OverlapableSerialList<Curve> original;
    OverlapableSerialList<Curve> edited;
    OverlapableSerialList<Curve> envelope;
};

class Params {
public:
    enum ParamType { Pitch, Energy, Tension, Breathiness };
    Param pitch;
    Param energy;
    Param tension;
    Param breathiness;
};



#endif //DSPARAMS_H
