//
// Created by fluty on 2024/2/7.
//

#ifndef HISTORYMANAGER_H
#define HISTORYMANAGER_H

#define historyManager HistoryManager::instance()

#include "Utils/Singleton.h"

#include <QObject>

class HistoryManagerPrivate;
class ActionSequence;

class HistoryManager final : public QObject {
    Q_OBJECT

    explicit HistoryManager(QObject *parent = nullptr);
    ~HistoryManager() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(HistoryManager)
    Q_DISABLE_COPY_MOVE(HistoryManager)

    void undo();
    void redo();
    void record(ActionSequence *actions);
    void reset();

    bool isOnSavePoint() const;
    void setSavePoint();
    bool canUndo() const;
    bool canRedo() const;
    QString undoActionName() const;
    QString redoActionName() const;

signals:
    void undoRedoChanged(bool canUndo, const QString &undoName, bool canRedo,
                         const QString &redoName);

private:
    Q_DECLARE_PRIVATE(HistoryManager)
    HistoryManagerPrivate *d_ptr;
};



#endif // HISTORYMANAGER_H
