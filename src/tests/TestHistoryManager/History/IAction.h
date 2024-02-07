//
// Created by fluty on 2024/2/7.
//

#ifndef IACTION_H
#define IACTION_H

class IAction {
public:
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual ~IAction() = default;
};

#endif // IACTION_H
