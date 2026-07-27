//
// Created by fluty on 24-2-23.
//

#ifndef PARAMSACTIONS_H
#define PARAMSACTIONS_H

#include <lite/ProjectModel/AppModel/Params.h>
#include <lite/History/ActionSequence.h>

class SingingClip;

class ParamsActions : public ActionSequence {
public:
    void replaceParam(ParamInfo::Name paramName, Param::Type paramType,
                      const QList<Curve *> &curves, SingingClip *clip);
};

#endif // PARAMSACTIONS_H
