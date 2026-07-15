#include "Model/AppStatus/AppStatus.h"
#include "Modules/History/ActionSequence.h"
#include "Modules/History/HistoryManager.h"
#include "Utils/ConditionalTransition.h"

#include <QCoreApplication>
#include <QState>
#include <QStateMachine>
#include <QTextStream>

template <>
AppStatus *AppContext::instance<AppStatus>() {
    return nullptr;
}

template <>
HistoryManager *AppContext::instance<HistoryManager>() {
    return nullptr;
}

namespace {
    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    class CountingAction final : public IAction {
    public:
        CountingAction(int &executeCount, int &undoCount, int &destroyCount)
            : m_executeCount(executeCount), m_undoCount(undoCount), m_destroyCount(destroyCount) {
        }

        ~CountingAction() override {
            ++m_destroyCount;
        }

        void execute() override {
            ++m_executeCount;
        }

        void undo() override {
            ++m_undoCount;
        }

    private:
        int &m_executeCount;
        int &m_undoCount;
        int &m_destroyCount;
    };

    class TestActionSequence final : public ActionSequence {
    public:
        using ActionSequence::addAction;
        using ActionSequence::setName;
    };

    class TransitionEmitter final : public QObject {
        Q_OBJECT

    signals:
        void proceed();
    };

    bool verifyGuardedBranch(const bool condition) {
        QStateMachine machine;
        const auto initial = new QState;
        const auto whenTrue = new QState;
        const auto whenFalse = new QState;
        machine.addState(initial);
        machine.addState(whenTrue);
        machine.addState(whenFalse);
        machine.setInitialState(initial);

        TransitionEmitter emitter;
        const auto trueTransition = new ConditionalTransition(&emitter, SIGNAL(proceed()),
                                                              [condition] { return condition; });
        trueTransition->setTargetState(whenTrue);
        initial->addTransition(trueTransition);
        const auto falseTransition = new ConditionalTransition(&emitter, SIGNAL(proceed()),
                                                               [condition] { return !condition; });
        falseTransition->setTargetState(whenFalse);
        initial->addTransition(falseTransition);

        machine.start();
        QCoreApplication::processEvents();
        emit emitter.proceed();
        QCoreApplication::processEvents();
        return condition ? whenTrue->active() : whenFalse->active();
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    auto manager = historyManager;
    bool ok = true;

    manager->reset(HistoryManager::ResetState::Saved);
    ok &= expect(manager->isOnSavePoint(), "saved reset must establish a saved baseline");

    manager->reset(HistoryManager::ResetState::Unsaved);
    ok &= expect(!manager->isOnSavePoint(), "unsaved reset must remain dirty with empty history");
    manager->setSavePoint();
    ok &= expect(manager->isOnSavePoint(), "setSavePoint must clear the unsaved baseline");

    int executeCount = 0;
    int undoCount = 0;
    int destroyCount = 0;
    auto sequence = new TestActionSequence;
    sequence->setName(QStringLiteral("Import MIDI"));
    sequence->addAction(new CountingAction(executeCount, undoCount, destroyCount));
    sequence->execute();
    manager->record(sequence);

    ok &= expect(executeCount == 1, "the import sequence must execute once before recording");
    ok &= expect(manager->canUndo(), "recorded import must be undoable");
    ok &= expect(manager->undoActionName() == QStringLiteral("Import MIDI"),
                 "the import sequence must keep its user-facing name");
    ok &= expect(!manager->isOnSavePoint(), "recording import must make the document dirty");

    manager->undo();
    ok &= expect(undoCount == 1, "one undo must undo the complete import sequence once");
    ok &= expect(!manager->canUndo() && manager->canRedo(),
                 "undo must transfer the complete import sequence to redo");
    ok &= expect(manager->isOnSavePoint(), "undo must return to the saved baseline");

    manager->redo();
    ok &= expect(executeCount == 2, "one redo must restore the complete import sequence once");
    ok &= expect(manager->canUndo() && !manager->canRedo(),
                 "redo must transfer the complete import sequence back to undo");

    manager->reset(HistoryManager::ResetState::Saved);
    ok &= expect(destroyCount == 1, "history reset must release the action it owns exactly once");
    ok &= expect(manager->isOnSavePoint(), "saved reset after import must be clean");
    ok &= expect(verifyGuardedBranch(true), "true guard must select the true target state");
    ok &= expect(verifyGuardedBranch(false), "false guard must select the false target state");

    return ok ? 0 : 1;
}

#include "main.moc"
