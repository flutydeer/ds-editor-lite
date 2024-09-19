//
// Created by fluty on 24-2-23.
//

#include "ParamsActions.h"

#include "ReplaceParamAction.h"

void ParamsActions::replaceParam(ParamInfo::Name paramName, Param::Type paramType,
                                 const QList<Curve *> &curves, SingingClip *clip) {
    addAction(ReplaceParamAction::build(paramName, paramType, curves, clip));
}