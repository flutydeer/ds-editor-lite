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

    const OverlapableSerialList<Curve> &curves(ParamType type) const;
    void setCurves(ParamType type, const OverlapableSerialList<Curve> &curves);
    void setCurves(ParamType type, const QList<Curve *> &curves);

private:
    OverlapableSerialList<Curve> m_original;
    OverlapableSerialList<Curve> m_edited;
    OverlapableSerialList<Curve> m_envelope;
    OverlapableSerialList<Curve> m_unknown;
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
