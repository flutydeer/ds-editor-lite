#include "tst_automation_protocol.h"

#include <lite/AutomationWire/McpProtocol.h>

#include <QtTest>

#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QPointer>
#include <QSemaphore>
#include <QThread>

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

// Test-only access to the worker thread handle. The stop handshake can only be
// held open around the worker thread, so these cases park a fence object on that
// thread and then observe the server while the thread is still alive.
#define private public
#include "Automation/Mcp/McpHttpServer.h"
#undef private

namespace {
    namespace Mcp = AutomationWire::Mcp;

    constexpr int handshakeTimeoutMs = 2000;
    constexpr int workerGateDelayMs = 100;

    // Keeps the handshake timeout short so a regression fails an assertion instead
    // of waiting for the production default, and restores the process afterwards.
    class HandshakeSettings final {
    public:
        HandshakeSettings() : m_previous(qgetenv("DS_EDITOR_SHUTDOWN_TIMEOUT_MS")) {
            qputenv("DS_EDITOR_SHUTDOWN_TIMEOUT_MS", QByteArray::number(handshakeTimeoutMs));
        }

        ~HandshakeSettings() {
            if (m_previous.isEmpty())
                qunsetenv("DS_EDITOR_SHUTDOWN_TIMEOUT_MS");
            else
                qputenv("DS_EDITOR_SHUTDOWN_TIMEOUT_MS", m_previous);
        }

        HandshakeSettings(const HandshakeSettings &) = delete;
        HandshakeSettings &operator=(const HandshakeSettings &) = delete;

    private:
        QByteArray m_previous;
    };

    // Holds the worker thread at a gate until the test releases it, which keeps the
    // stop acknowledgement pending for as long as the assertions need it.
    class WorkerGate final {
    public:
        explicit WorkerGate(QThread *workerThread) {
            m_fence->moveToThread(workerThread);
            QPointer<QObject> fence = m_fence;
            const auto atGate = m_atGate;
            const auto releaseGate = m_releaseGate;
            QMetaObject::invokeMethod(
                m_fence,
                [fence, atGate, releaseGate] {
                    atGate->release();
                    releaseGate->acquire();
                    if (fence)
                        fence->moveToThread(QCoreApplication::instance()->thread());
                },
                Qt::QueuedConnection);
        }

        ~WorkerGate() {
            release();
            if (!m_fence)
                return;
            if (m_fence->thread() == QThread::currentThread()) {
                delete m_fence.data();
                return;
            }
            // The worker thread still owns the fence, so the object is left to the
            // process instead of being destroyed under a running thread.
        }

        WorkerGate(const WorkerGate &) = delete;
        WorkerGate &operator=(const WorkerGate &) = delete;

        bool waitAtGate(const int timeoutMs = handshakeTimeoutMs) {
            return m_atGate->tryAcquire(1, timeoutMs);
        }

        void release() {
            m_releaseGate->release();
        }

    private:
        QPointer<QObject> m_fence = new QObject;
        std::shared_ptr<QSemaphore> m_atGate = std::make_shared<QSemaphore>();
        std::shared_ptr<QSemaphore> m_releaseGate = std::make_shared<QSemaphore>();
    };

    // Releases the gate from an independent thread. The owner thread is blocked
    // inside the stop while the gate is closed, and no test-created dependency
    // between the owner and the worker may be part of what is measured.
    class GateReleaser final {
    public:
        GateReleaser(WorkerGate &gate, const int delayMs)
            : m_thread([&gate, delayMs] {
                  QThread::msleep(delayMs);
                  gate.release();
              }) {
        }

        ~GateReleaser() {
            if (m_thread.joinable())
                m_thread.join();
        }

        GateReleaser(const GateReleaser &) = delete;
        GateReleaser &operator=(const GateReleaser &) = delete;

    private:
        std::thread m_thread;
    };

    // Releases the gate from an independent thread after the probe signalled that it
    // ran, or after a fallback delay when the probe never gets to run because a stale
    // finalizer is blocking the owner thread. The fallback stays below the server's
    // minimum handshake window, so an implementation that joins the next stop is still
    // observed instead of hanging the case.
    class ProbeGateReleaser final {
    public:
        ProbeGateReleaser(std::shared_ptr<WorkerGate> gate, std::shared_ptr<QSemaphore> probeDone,
                          const int fallbackMs)
            : m_thread([gate = std::move(gate), probeDone, fallbackMs] {
                  probeDone->tryAcquire(1, fallbackMs);
                  gate->release();
              }) {
        }

        ~ProbeGateReleaser() {
            if (m_thread.joinable())
                m_thread.join();
        }

        ProbeGateReleaser(const ProbeGateReleaser &) = delete;
        ProbeGateReleaser &operator=(const ProbeGateReleaser &) = delete;

    private:
        std::thread m_thread;
    };

    QJsonObject unusedResponse(const Mcp::RequestEnvelope &, const QString &) {
        return {};
    }

    bool pumpUntil(const bool &condition, QElapsedTimer &elapsed, const int timeoutMs) {
        while (!condition && elapsed.elapsed() < timeoutMs)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        return condition;
    }
}

void AutomationProtocolTests::synchronousStopKeepsOwnerEventsQueued() {
    HandshakeSettings handshake;
    Automation::McpHttpServer server(this, unusedResponse);
    QString error;
    QVERIFY2(server.start(0, error), qPrintable(error));
    QVERIFY(server.m_thread);

    {
        // The gate is closed before the stop is requested, so the join below can
        // only return once the worker was released and acknowledged the stop.
        WorkerGate gate(server.m_thread);
        QVERIFY2(gate.waitAtGate(), "the worker must reach its gate before the stop");

        server.requestStop();
        QVERIFY(server.isStopping());

        auto ownerEventRan = std::make_shared<bool>(false);
        QMetaObject::invokeMethod(
            this, [ownerEventRan] { *ownerEventRan = true; }, Qt::QueuedConnection);
        GateReleaser releaser(gate, workerGateDelayMs);

        server.stop();

        QVERIFY2(!server.isStopping(),
                 "a synchronous stop must join the worker inside its handshake timeout");
        QVERIFY2(!*ownerEventRan,
                 "a synchronous stop must not run queued owner-thread callbacks while it waits");
        QVERIFY(!server.isListening());
        QVERIFY(server.endpoint().isEmpty());

        QCoreApplication::processEvents();
        QVERIFY2(*ownerEventRan,
                 "callbacks queued before the stop must still run after it, not be dropped");
    }
}

void AutomationProtocolTests::queuedStopRetiresThePendingStopExactlyOnce() {
    HandshakeSettings handshake;
    Automation::McpHttpServer server(this, unusedResponse);
    QString error;
    QVERIFY2(server.start(0, error), qPrintable(error));
    QVERIFY(server.m_thread);

    const auto stoppedCount = std::make_shared<int>(0);
    const auto trackStop = connect(&server, &Automation::McpHttpServer::stopped, this,
                                   [stoppedCount] { ++*stoppedCount; });

    {
        WorkerGate gate(server.m_thread);
        QVERIFY2(gate.waitAtGate(), "the worker must reach its gate before the stop");
        server.requestStop();
        QVERIFY(server.isStopping());

        // Mirrors the editor's shutdown handler: it stops the server again while the
        // stop requested above is still pending. The state is shared and the server
        // is guarded, so a late invocation cannot touch a finished test.
        const auto nestedStopRan = std::make_shared<bool>(false);
        const auto nestedStopRetiredTheWorker = std::make_shared<bool>(false);
        const QPointer<Automation::McpHttpServer> serverGuard(&server);
        QMetaObject::invokeMethod(
            this,
            [serverGuard, nestedStopRan, nestedStopRetiredTheWorker] {
                *nestedStopRan = true;
                if (!serverGuard)
                    return;
                if (serverGuard->isListening() || serverGuard->isStopping()) {
                    serverGuard->requestStop();
                    serverGuard->stop();
                }
                *nestedStopRetiredTheWorker = !serverGuard->isStopping();
            },
            Qt::QueuedConnection);

        GateReleaser releaser(gate, workerGateDelayMs);
        QElapsedTimer elapsed;
        elapsed.start();
        QVERIFY2(pumpUntil(*nestedStopRan, elapsed, handshakeTimeoutMs + 1000),
                 "the queued shutdown callback must run while the stop is pending");
        QVERIFY2(*nestedStopRetiredTheWorker,
                 "a stop requested while stopping must retire the worker itself instead of "
                 "waiting for a pending finalizer");
        QVERIFY(!server.isListening() && !server.isStopping());
    }

    // The finalizer the worker queued before it exited must retire nothing twice.
    for (int round = 0; round < 5; ++round)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    QCOMPARE(*stoppedCount, 1);
    disconnect(trackStop);
}

void AutomationProtocolTests::staleStopFinalizerDoesNotRetireTheNextStop() {
    HandshakeSettings handshake;
    Automation::McpHttpServer server(this, unusedResponse);
    QString error;
    QVERIFY2(server.start(0, error), qPrintable(error));

    // Shared state: every queued callback keeps its own reference, so a failing
    // assertion that returns early cannot leave an event pointing at this stack.
    const auto stops = std::make_shared<int>(0);
    const auto trackStop =
        connect(&server, &Automation::McpHttpServer::stopped, this, [stops] { ++*stops; });

    // First generation: let the worker acknowledge its stop so its finalizer is queued
    // to this thread, then finish that stop synchronously. Nothing pumps this thread in
    // between, so the queued finalizer is still stale when the next stop begins.
    {
        WorkerGate gate(server.m_thread);
        QVERIFY2(gate.waitAtGate(), "the first worker must reach its gate before its stop");
        server.requestStop();
        gate.release();
        QVERIFY2(server.m_thread->wait(handshakeTimeoutMs),
                 "the first worker must acknowledge its stop");
    }
    server.stop();
    QCOMPARE(*stops, 1);
    QVERIFY(!server.isListening() && !server.isStopping());

    QVERIFY2(server.start(0, error), qPrintable(error));
    const auto nextGate = std::make_shared<WorkerGate>(server.m_thread);
    QVERIFY2(nextGate->waitAtGate(), "the second worker must reach its gate before its stop");
    server.requestStop();
    QVERIFY(server.isStopping());

    const auto probeRan = std::make_shared<bool>(false);
    const auto probeDone = std::make_shared<QSemaphore>();
    const auto stopsSeenByProbe = std::make_shared<int>(-1);
    const auto pendingSeenByProbe = std::make_shared<bool>(false);
    const QPointer<Automation::McpHttpServer> serverGuard(&server);

    // The probe runs after the stale finalizer in event order and checks the state
    // before releasing the gate, so no timing race can let the second generation finish
    // first. Only a stale finalizer that blocks this thread keeps the gate closed.
    QMetaObject::invokeMethod(
        this,
        [probeRan, probeDone, stopsSeenByProbe, pendingSeenByProbe, serverGuard, stops, nextGate] {
            if (serverGuard) {
                *stopsSeenByProbe = *stops;
                *pendingSeenByProbe = serverGuard->isStopping();
            }
            *probeRan = true;
            nextGate->release();
            probeDone->release();
        },
        Qt::QueuedConnection);

    // Fallback for the implementation that joins the next stop from the stale finalizer:
    // the probe cannot run until that join gives up, so the gate is released below the
    // server's minimum handshake window and the probe then observes that takeover.
    ProbeGateReleaser releaser(nextGate, probeDone, handshakeTimeoutMs);

    QElapsedTimer elapsed;
    elapsed.start();
    QVERIFY2(pumpUntil(*probeRan, elapsed, handshakeTimeoutMs + 2000),
             "the probe queued after the stale finalizer must run");
    QCOMPARE(*stopsSeenByProbe, 1);
    QVERIFY2(*pendingSeenByProbe,
             "the finalizer of a retired worker must not retire the next stop");

    // The second generation still retires its own worker and reports it exactly once.
    while (server.isStopping() && elapsed.elapsed() < handshakeTimeoutMs + 2000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    QVERIFY2(!server.isStopping(), "the second generation must retire its own worker");
    QCOMPARE(*stops, 2);
    disconnect(trackStop);
}
