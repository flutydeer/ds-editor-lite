//
// Created by fluty on 24-2-23.
//

#ifndef PARAMSACTIONS_H
#define PARAMSACTIONS_H

#include "Model/Params.h"
#include "Modules/History/ActionSequence.h"

class SingingClip;

class ParamsActions : public ActionSequence {
public:
    void replaceParam(ParamBundle::ParamName paramName, Param::ParamType paramType,
                      const OverlappableSerialList<Curve> &curves, SingingClip* clip);
    void replacePitchOriginal(const OverlappableSerialList<Curve> &curves, SingingClip* clip);
    void replacePitchEdited(const OverlappableSerialList<Curve> &curves, SingingClip* clip);
};

#endif // PARAMSACTIONS_H
