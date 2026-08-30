#include "UndoRedoController.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "EditorViewController.h"
#include "Model/AppStatus/AppStatus.h"
#include <lite/History/ActionSequence.h>
#include <lite/History/HistoryManager.h>

LITE_SINGLETON_IMPLEMENT_INSTANCE(UndoRedoController)

UndoRedoController::UndoRedoController(QObject *parent) : QObject(parent) {
    connect(historyManager, &HistoryManager::undoRedoChanged, this,
            [this](bool, const QString &, bool, const QString &) { clearPending(); });
}

void UndoRedoController::requestUndo() {
    request(Direction::Undo);
}

void UndoRedoController::requestRedo() {
    request(Direction::Redo);
}

void UndoRedoController::request(const Direction direction) {
    if (m_pending.has_value() && m_pending->direction != direction)
        clearPending();
    const auto sequence = direction == Direction::Undo ? historyManager->nextUndoEntry()
                                                       : historyManager->nextRedoEntry();
    if (!sequence)
        return;

    if (appStatus->currentEditObject != AppStatus::EditObjectType::None) {
        // An edit is still in progress: refuse undo/redo. This guard used to
        // live inside HistoryManager::undo()/redo(); it moved here so the
        // history library stays free of AppStatus (app-runtime state).
        clearPending();
        return;
    }

    if (!sequence->focusTransition().has_value()) {
        clearPending();
        commit(direction);
        return;
    }

    const auto &transition = sequence->focusTransition().value();
    const auto &currentFocus = direction == Direction::Undo ? transition.after : transition.before;
    const auto &resultFocus = direction == Direction::Undo ? transition.before : transition.after;
    const auto isSamePending = m_pending.has_value() &&
                               m_pending->historyId == sequence->historyId() &&
                               m_pending->direction == direction;

    const auto visibility = editorViewController->focusVisibility(currentFocus);
    if (visibility == HistoryFocusVisibility::Unavailable) {
        execute(direction, sequence, resultFocus);
        return;
    }

    const bool needsNavigation = visibility == HistoryFocusVisibility::ScrollRequired ||
                                 visibility == HistoryFocusVisibility::ContextSwitchRequired;
    if (!isSamePending && needsNavigation) {
        clearPending();
        if (editorViewController->revealFocus(currentFocus)) {
            m_pending = Pending{sequence->historyId(), direction};
            emit focusNavigationRequested(direction == Direction::Undo);
            return;
        }
        execute(direction, sequence, resultFocus);
        return;
    }

    execute(direction, sequence, resultFocus);
}

void UndoRedoController::execute(const Direction direction, const ActionSequence *sequence,
                                 const HistoryFocus &resultFocus) {
    Q_UNUSED(sequence);
    clearPending();
    commit(direction);
    if (resultFocus.isValid())
        editorViewController->finalizeFocus(resultFocus);
}

void UndoRedoController::commit(const Direction direction) {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return;
    Automation::CommandContext context{.expected = runtime->documentVersion()};
    if (direction == Direction::Undo)
        runtime->history().undo(context);
    else
        runtime->history().redo(context);
}

void UndoRedoController::clearPending() {
    if (m_pending.has_value()) {
        m_pending.reset();
        editorViewController->clearFocusPreview();
    }
}
