//
// Created by fluty on 24-2-23.
//

#ifndef PARAMSACTIONS_H
#define PARAMSACTIONS_H

#include "Model/AppModel/Params.h"
#include "Modules/History/ActionSequence.h"

class SingingClip;

class ParamsActions : public ActionSequence {
public:
    void replaceParam(ParamInfo::Name paramName, Param::Type paramType,
                      const QList<Curve *> &curves, SingingClip *clip);
    void replacePitchOriginal(const QList<Curve *> &curves, SingingClip *clip);
    void replacePitchEdited(const QList<Curve *> &curves, SingingClip *clip);
};

#endif // PARAMSACTIONS_H
