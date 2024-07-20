//
// Created by fluty on 2024/2/7.
//

#ifndef HISTORYMANAGER_H
#define HISTORYMANAGER_H

#include <QObject>
#include <QStack>

#include "ActionSequence.h"
#include "Utils/Singleton.h"

class HistoryManager final : public QObject, public Singleton<HistoryManager> {
    Q_OBJECT

public:
    void undo();
    void redo();
    void record(ActionSequence *actions);
    void reset();

    [[nodiscard]] bool isOnSavePoint() const;
    void setSavePoint();
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] QString undoActionName() const;
    [[nodiscard]] QString redoActionName() const;

signals:
    void undoRedoChanged(bool canUndo, const QString &undoName, bool canRedo,
                       const QString &redoName);

private:
    QStack<ActionSequence *> m_undoStack;
    QStack<ActionSequence *> m_redoStack;
    ActionSequence *m_savePoint = nullptr;
    bool m_isSavePointSet = false;
};



#endif // HISTORYMANAGER_H
