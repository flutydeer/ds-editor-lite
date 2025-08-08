//
// Created by fluty on 24-2-23.
//

#include "ParamsActions.h"

#include "ReplaceParamAction.h"

void ParamsActions::replaceParam(const ParamInfo::Name paramName, Param::Type paramType,
                                 const QList<Curve *> &curves, SingingClip *clip) {
    addAction(new ReplaceParamAction(paramName, paramType, curves, clip));
}