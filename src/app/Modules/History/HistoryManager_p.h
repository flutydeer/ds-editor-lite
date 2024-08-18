//
// Created by fluty on 24-7-21.
//

#ifndef HISTORYMANAGERPRIVATE_H
#define HISTORYMANAGERPRIVATE_H

#include <QStack>

class ActionSequence;
class HistoryManager;

class HistoryManagerPrivate {
    Q_DECLARE_PUBLIC(HistoryManager);

public:
    QStack<ActionSequence *> m_undoStack;
    QStack<ActionSequence *> m_redoStack;
    ActionSequence *m_savePoint = nullptr;
    bool m_isSavePointSet = false;

private:
    HistoryManager *q_ptr = nullptr;
};



#endif // HISTORYMANAGERPRIVATE_H
