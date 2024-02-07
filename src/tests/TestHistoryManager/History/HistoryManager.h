//
// Created by fluty on 2024/2/7.
//

#ifndef HISTORYMANAGER_H
#define HISTORYMANAGER_H

#include <QObject>
#include <QStack>

#include "ActionSequence.h"
#include "../../gui/Utils/Singleton.h"

class HistoryManager : public QObject, public Singleton<HistoryManager> {
    Q_OBJECT

public:
    void undo();
    void redo();
    void record(ActionSequence *actions);

    bool canUndo() const;
    bool canRedo() const;

signals:
    void undoRedoChanged();

private:
    QStack<ActionSequence *> m_undoStack;
    QStack<ActionSequence *> m_redoStack;
};



#endif // HISTORYMANAGER_H
