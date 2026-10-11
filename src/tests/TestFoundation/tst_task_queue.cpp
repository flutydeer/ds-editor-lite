#include "tst_foundation.h"

#include <lite/Tasking/TaskQueue.h>

#include <QPointer>
#include <QScopeGuard>
#include <QSemaphore>
#include <QThread>
#include <QThreadPool>
#include <QtTest>

namespace {
    class CompletionDeletionProbe final : public QObject {
    public:
        explicit CompletionDeletionProbe(const std::atomic_bool &emitting) : emitting(emitting) {
        }

        bool deletionWhileEmitting = false;

    protected:
        bool eventFilter(QObject *, QEvent *event) override {
            if (event->type() == QEvent::DeferredDelete && emitting.load()) {
                // Observe unsafe deletion without crashing the test's emitting thread.
                deletionWhileEmitting = true;
                return true;
            }
            return false;
        }

    private:
        const std::atomic_bool &emitting;
    };

    class ControlledCompletionTask final : public Task {
    public:
        bool stopped() const {
            const auto wasStopped = Task::stopped();
            if (finishAtObservation) {
                finishAtObservation = false;
                release.release();
                // Expose completion between observing the flag and subscribing to finished().
                completionObserved = QThreadPool::globalInstance()->waitForDone(3000);
            }
            return wasStopped;
        }

        mutable bool finishAtObservation = false;
        mutable bool completionObserved = false;
        mutable QSemaphore release;
        QSemaphore entered;

    protected:
        void runTask() override {
            entered.release();
            release.acquire();
        }
    };
}

void FoundationTests::canceledWorkerCompletionAdvancesTheQueue_data() {
    QTest::addColumn<bool>("alreadyStopped");
    QTest::addColumn<bool>("finishAtObservation");
    QTest::newRow("running") << false << false;
    QTest::newRow("already-finished") << true << false;
    QTest::newRow("finishes-during-cancel") << false << true;
}

void FoundationTests::canceledWorkerCompletionAdvancesTheQueue() {
    QFETCH(bool, alreadyStopped);
    QFETCH(bool, finishAtObservation);
    QVERIFY(taskManager->tasks().isEmpty());
    TaskQueue<ControlledCompletionTask> queue;
    QPointer<ControlledCompletionTask> first = new ControlledCompletionTask;
    QPointer<ControlledCompletionTask> next = new ControlledCompletionTask;
    const auto cleanup = qScopeGuard([&] {
        if (first)
            first->release.release();
        if (next)
            next->release.release();
        QThreadPool::globalInstance()->waitForDone();
        queue.cancelIf([](auto *) { return true; });
        if (queue.current && queue.current->Task::stopped())
            queue.onCurrentFinished(queue.current);
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    });
    QObject::connect(next, &Task::finished, this, [&] { queue.onCurrentFinished(next); });
    queue.add(first);
    QVERIFY(first->entered.tryAcquire(1, 3000));
    queue.add(next);
    QVERIFY(!next->started());
    if (alreadyStopped) {
        first->release.release();
        QVERIFY(QThreadPool::globalInstance()->waitForDone(3000));
        QTRY_VERIFY_WITH_TIMEOUT(first->Task::stopped(), 3000);
    }
    first->finishAtObservation = finishAtObservation;
    queue.cancelIf([&](auto *task) { return task == first; });
    QVERIFY(first->terminated());
    if (finishAtObservation)
        QVERIFY(first->completionObserved);
    if (!alreadyStopped && !finishAtObservation) {
        QCOMPARE(queue.current, first.data());
        QVERIFY(!next->started());
        first->release.release();
    }
    QTRY_VERIFY_WITH_TIMEOUT(next->started(), 3000);
    QCOMPARE(queue.current, next.data());
    next->release.release();
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 3000);
    QVERIFY(queue.current == nullptr);
    QVERIFY(queue.pending.count() == 0);
}

void FoundationTests::cancellationDoesNotDeleteAnEmittingWorker() {
    QVERIFY(taskManager->tasks().isEmpty());
    TaskQueue<ControlledCompletionTask> queue;
    QPointer<ControlledCompletionTask> task = new ControlledCompletionTask;
    std::atomic_bool emittingFromWorker{false};
    std::atomic_bool deliveredOnOwner{false};
    QSemaphore finishNotification;
    CompletionDeletionProbe probe(emittingFromWorker);
    task->installEventFilter(&probe);
    auto *owner = task->thread();
    QObject::connect(
        task, &Task::finished, this,
        [&] {
            if (QThread::currentThread() == owner) {
                deliveredOnOwner.store(true);
                return;
            }
            emittingFromWorker.store(true);
            finishNotification.acquire();
            emittingFromWorker.store(false);
        },
        Qt::DirectConnection);
    const auto cleanup = qScopeGuard([&] {
        if (task)
            task->release.release();
        finishNotification.release();
        QThreadPool::globalInstance()->waitForDone();
        queue.cancelIf([](auto *) { return true; });
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        if (task) {
            if (queue.current == task)
                queue.current = nullptr;
            if (taskManager->findTaskById(task->id()))
                taskManager->removeTask(task);
            delete task;
        }
    });
    queue.add(task);
    QVERIFY(task->entered.tryAcquire(1, 3000));
    task->release.release();
    QTRY_VERIFY_WITH_TIMEOUT(emittingFromWorker.load() || deliveredOnOwner.load(), 3000);
    queue.cancelIf([](auto *) { return true; });
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const auto unsafeDeletion = probe.deletionWhileEmitting;
    finishNotification.release();
    QVERIFY(QThreadPool::globalInstance()->waitForDone(3000));
    QVERIFY2(!unsafeDeletion, "A task must remain alive while its worker is emitting completion");
}
