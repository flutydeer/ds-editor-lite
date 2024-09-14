//
// Created by fluty on 2024/2/7.
//

#include "HistoryManager.h"
#include "HistoryManager_p.h"

#include <QDebug>

#include "ActionSequence.h"
#include "Model/AppStatus/AppStatus.h"

HistoryManager::HistoryManager() : d_ptr(new HistoryManagerPrivate) {
    Q_D(HistoryManager);
    d->q_ptr = this;
}

HistoryManager::~HistoryManager() {
    delete d_ptr;
}

void HistoryManager::undo() {
    Q_D(HistoryManager);
    if (appStatus->editing) {
        qWarning() << "Cannot undo because editing not finished";
        return;
    }
    if (d->m_undoStack.isEmpty())
        return;

    auto seq = d->m_undoStack.pop();
    seq->undo();
    d->m_redoStack.push(seq);

    emit undoRedoChanged(canUndo(), undoActionName(), canRedo(), redoActionName());
}

void HistoryManager::redo() {
    Q_D(HistoryManager);
    if (appStatus->editing) {
        qWarning() << "Cannot redo because editing not finished";
        return;
    }
    if (d->m_redoStack.isEmpty())
        return;

    auto seq = d->m_redoStack.pop();
    seq->execute();
    d->m_undoStack.push(seq);

    emit undoRedoChanged(canUndo(), undoActionName(), canRedo(), redoActionName());
}

void HistoryManager::record(ActionSequence *actions) {
    Q_D(HistoryManager);
    if (actions->count() <= 0)
        return;

    d->m_undoStack.push(actions);
    d->m_redoStack.clear();
    emit undoRedoChanged(canUndo(), undoActionName(), canRedo(), redoActionName());
}

void HistoryManager::reset() {
    Q_D(HistoryManager);
    qDebug() << "HistoryManager::reset()";
    for (auto seq : d->m_undoStack)
        delete seq;
    for (auto seq : d->m_redoStack)
        delete seq;
    d->m_undoStack.clear();
    d->m_redoStack.clear();
    d->m_savePoint = nullptr;
    d->m_isSavePointSet = false;
    emit undoRedoChanged(canUndo(), "", canRedo(), "");
}

bool HistoryManager::isOnSavePoint() const {
    Q_D(const HistoryManager);
    auto flag = false;
    if (d->m_undoStack.isEmpty() && d->m_redoStack.isEmpty())
        return true;

    if (!d->m_isSavePointSet)
        return flag;

    if (d->m_savePoint != nullptr) {
        if (!d->m_undoStack.isEmpty())
            if (d->m_undoStack.top() == d->m_savePoint)
                flag = true;
    } else if (d->m_undoStack.isEmpty())
        flag = true;
    return flag;
}

void HistoryManager::setSavePoint() {
    Q_D(HistoryManager);
    d->m_isSavePointSet = true;
    if (d->m_undoStack.isEmpty()) {
        d->m_savePoint = nullptr;
        return;
    }
    d->m_savePoint = d->m_undoStack.top();
}

bool HistoryManager::canUndo() const {
    Q_D(const HistoryManager);
    return !d->m_undoStack.isEmpty();
}

bool HistoryManager::canRedo() const {
    Q_D(const HistoryManager);
    return !d->m_redoStack.isEmpty();
}

QString HistoryManager::undoActionName() const {
    Q_D(const HistoryManager);
    return d->m_undoStack.isEmpty() ? "" : d->m_undoStack.top()->name();
}

QString HistoryManager::redoActionName() const {
    Q_D(const HistoryManager);
    return d->m_redoStack.isEmpty() ? "" : d->m_redoStack.top()->name();
}