//
// Created by fluty on 24-2-23.
//

#include "ReplaceParamAction.h"

#include "Model/AppModel/SingingClip.h"
#include "Utils/AppModelUtils.h"

ReplaceParamAction *ReplaceParamAction::build(ParamInfo::Name paramName,
                                              Param::Type paramType,
                                              const QList<Curve *> &curves, SingingClip *clip) {
    auto a = new ReplaceParamAction;
    a->m_paramName = paramName;
    a->m_paramType = paramType;
    AppModelUtils::copyCurves(clip->params.getParamByName(paramName)->curves(paramType),
                              a->m_oldCurves);
    AppModelUtils::copyCurves(curves, a->m_newCurves);
    a->m_clip = clip;
    return a;
}

void ReplaceParamAction::execute() {
    auto curves = m_clip->params.getParamByName(m_paramName);
    curves->setCurves(m_paramType, m_newCurves);
    m_clip->notifyParamChanged(m_paramName, m_paramType);
}

void ReplaceParamAction::undo() {
    auto curves = m_clip->params.getParamByName(m_paramName);
    curves->setCurves(m_paramType, m_oldCurves);
    m_clip->notifyParamChanged(m_paramName, m_paramType);
}