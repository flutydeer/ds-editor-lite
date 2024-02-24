//
// Created by fluty on 24-2-23.
//

#ifndef REPLACEPARAMACTION_H
#define REPLACEPARAMACTION_H

#include "Controller/History/IAction.h"
#include "Model/Params.h"

class SingingClip;

class ReplaceParamAction final : public IAction {
public:
    static ReplaceParamAction *build(ParamBundle::ParamName paramName, Param::ParamType paramType,
                                     const OverlapableSerialList<Curve> &curves, SingingClip *clip);
    void execute() override;
    void undo() override;

private:
    ParamBundle::ParamName m_paramName = ParamBundle::Unknown;
    Param::ParamType m_paramType = Param::ParamType::Unknown;
    OverlapableSerialList<Curve> m_oldCurves;
    OverlapableSerialList<Curve> m_newCurves;
    SingingClip *m_clip = nullptr;
};



#endif // REPLACEPARAMACTION_H
