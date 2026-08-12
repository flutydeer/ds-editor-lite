#include <lite/Tasking/Task.h>
#include <lite/Tasking/TaskManager.h>

#include <QCoreApplication>
#include <QTextStream>
#include <QUuid>

namespace {
    class PendingTask final : public Task {
    protected:
        void runTask() override {
        }
    };

    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    auto *manager = TaskManager::instance();
    const auto firstId = QUuid::createUuid();
    const auto secondId = QUuid::createUuid();
    QUuid currentId = firstId;
    manager->setDocumentIdProvider([&currentId] { return currentId; });

    PendingTask first;
    PendingTask second;
    manager->addTask(&first);
    currentId = secondId;
    manager->addTask(&second);

    bool ok = true;
    ok &= expect(first.documentId() == firstId, "first task must capture the active document ID");
    ok &=
        expect(second.documentId() == secondId, "second task must capture the changed document ID");
    ok &= expect(manager->tasksForDocument(firstId) == QList<Task *>{&first},
                 "first document query must not include another document's task");
    ok &= expect(manager->tasksForDocument(secondId) == QList<Task *>{&second},
                 "second document query must not include another document's task");

    manager->terminateTasks(firstId);
    ok &= expect(first.terminated(), "terminating a document must terminate its task");
    ok &= expect(!second.terminated(), "terminating a document must not affect another document");
    manager->waitForDocument(firstId);

    manager->removeTask(&first);
    manager->removeTask(&second);
    manager->setDocumentIdProvider({});
    return ok ? 0 : 1;
}
