//
// Created by fluty on 24-2-23.
//

#include "ReplaceParamAction.h"

#include "Model/Clip.h"

ReplaceParamAction *ReplaceParamAction::build(ParamBundle::ParamName paramName,
                                              Param::ParamType paramType,
                                              const OverlappableSerialList<Curve> &curves,
                                              SingingClip *clip) {
    auto a = new ReplaceParamAction;
    a->m_paramName = paramName;
    a->m_paramType = paramType;
    SingingClip::copyCurves(clip->params.getParamByName(paramName)->curves(paramType), a->m_oldCurves);
    SingingClip::copyCurves(curves, a->m_newCurves);
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