#ifndef UNDOREDOCONTROLLER_H
#define UNDOREDOCONTROLLER_H

#define undoRedoController UndoRedoController::instance()

#include "Utils/Singleton.h"

#include <QObject>
#include <optional>

class ActionSequence;
struct HistoryFocus;

class UndoRedoController final : public QObject {
    Q_OBJECT

private:
    explicit UndoRedoController(QObject *parent = nullptr);
    ~UndoRedoController() override = default;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(UndoRedoController)
    Q_DISABLE_COPY_MOVE(UndoRedoController)

public slots:
    void requestUndo();
    void requestRedo();

signals:
    void focusNavigationRequested(bool undo);

private:
    enum class Direction { Undo, Redo };

    struct Pending {
        quint64 historyId = 0;
        Direction direction = Direction::Undo;
    };

    void request(Direction direction);
    void execute(Direction direction, const ActionSequence *sequence,
                 const HistoryFocus &resultFocus);
    void clearPending();

    std::optional<Pending> m_pending;
};

#endif // UNDOREDOCONTROLLER_H
