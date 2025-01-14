//
// Created by fluty on 24-2-23.
//

#ifndef REPLACEPARAMACTION_H
#define REPLACEPARAMACTION_H

#include "Model/AppModel/Params.h"
#include "Modules/History/IAction.h"

class SingingClip;

class ReplaceParamAction final : public IAction {
public:
    explicit ReplaceParamAction(ParamInfo::Name paramName, Param::Type paramType,
                                const QList<Curve *> &curves, SingingClip *clip);
    void execute() override;
    void undo() override;

private:
    ParamInfo::Name m_paramName = ParamInfo::Unknown;
    Param::Type m_paramType = Param::Type::Unknown;
    QList<Curve *> m_oldCurves;
    QList<Curve *> m_newCurves;
    SingingClip *m_clip = nullptr;
};



#endif // REPLACEPARAMACTION_H
