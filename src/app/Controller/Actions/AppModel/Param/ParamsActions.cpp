//
// Created by fluty on 24-2-23.
//

#include "ParamsActions.h"

#include "ReplaceParamAction.h"

void ParamsActions::replaceParam(ParamBundle::ParamName paramName, Param::ParamType paramType,
                                 const QList<Curve *> &curves, SingingClip *clip) {
    addAction(ReplaceParamAction::build(paramName, paramType, curves, clip));
}
void ParamsActions::replacePitchOriginal(const QList<Curve *> &curves, SingingClip *clip) {
    addAction(ReplaceParamAction::build(ParamBundle::Pitch, Param::Original, curves, clip));
}
void ParamsActions::replacePitchEdited(const QList<Curve *> &curves, SingingClip *clip) {
    addAction(ReplaceParamAction::build(ParamBundle::Pitch, Param::Edited, curves, clip));
}