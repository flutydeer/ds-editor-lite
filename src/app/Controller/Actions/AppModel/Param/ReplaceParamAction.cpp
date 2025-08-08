//
// Created by fluty on 24-2-23.
//

#include "ReplaceParamAction.h"

#include "Model/AppModel/SingingClip.h"
#include "Utils/AppModelUtils.h"

ReplaceParamAction::ReplaceParamAction(const ParamInfo::Name paramName, const Param::Type paramType,
                                       const QList<Curve *> &curves, SingingClip *clip)
    : m_paramName(paramName), m_paramType(paramType), m_clip(clip) {
    AppModelUtils::copyCurves(clip->params.getParamByName(paramName)->curves(paramType),
                              m_oldCurves);
    AppModelUtils::copyCurves(curves, m_newCurves);
}

void ReplaceParamAction::execute() {
    const auto param = m_clip->params.getParamByName(m_paramName);
    param->setCurves(m_paramType, m_newCurves, m_clip);
    m_clip->notifyParamChanged(m_paramName, m_paramType);
}

void ReplaceParamAction::undo() {
    const auto param = m_clip->params.getParamByName(m_paramName);
    param->setCurves(m_paramType, m_oldCurves, m_clip);
    m_clip->notifyParamChanged(m_paramName, m_paramType);
}