//
// Created by fluty on 2024/7/11.
//

#ifndef MOVETRACKACTION_H
#define MOVETRACKACTION_H

#include "Modules/History/IAction.h"

#include <QtTypes>

class AppModel;

class MoveTrackAction final : public IAction {
public:
    static MoveTrackAction *build(qsizetype fromIndex, qsizetype toIndex, AppModel *model);
    void execute() override;
    void undo() override;

private:
    qsizetype m_fromIndex = -1;
    qsizetype m_toIndex = -1;
    qsizetype m_actualToIndex = -1; // The actual index after adjustment
    AppModel *m_model = nullptr;
};

#endif // MOVETRACKACTION_H
