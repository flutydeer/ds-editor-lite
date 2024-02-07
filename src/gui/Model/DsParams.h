//
// Created by fluty on 2024/1/27.
//

#ifndef DSPARAMS_H
#define DSPARAMS_H

#include "DsCurve.h"
#include "../Utils/OverlapableSerialList.h"

class DsParam {
public:
    OverlapableSerialList<DsCurve> original;
    OverlapableSerialList<DsCurve> edited;
    OverlapableSerialList<DsCurve> envelope;
};

class DsParams {
public:
    DsParam pitch;
    DsParam energy;
    DsParam tension;
    DsParam breathiness;
};



#endif //DSPARAMS_H
