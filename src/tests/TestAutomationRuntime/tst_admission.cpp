#include "tst_automation_runtime.h"

#include "Automation/Public/AdmissionController.h"
#include "Automation/CoreRuntime.h"

#include <QList>
#include <QtTest>

void AutomationRuntimeTests::capacity_data() {
    QTest::addColumn<int>("capacity");
    QTest::newRow("single-request") << 1;
    QTest::newRow("default-capacity") << Automation::AdmissionLimits{}.maximumGlobalInFlight;
}

void AutomationRuntimeTests::capacity() {
    QFETCH(int, capacity);
    Automation::AdmissionLimits limits;
    limits.maximumGlobalInFlight = capacity;
    Automation::AdmissionController controller(limits);
    QList<Automation::AdmissionLease> leases;
    for (int index = 0; index < capacity; ++index) {
        auto result = controller.tryAcquire();
        QVERIFY(result);
        leases.append(std::move(result.get()));
    }
    const auto full = controller.tryAcquire();
    QVERIFY(!full);
    QCOMPARE(full.getError().code, Automation::AutomationErrorCode::Busy);
    QCOMPARE(controller.snapshot().globalInFlight, capacity);
    leases.removeLast();
    QVERIFY(controller.tryAcquire());
}

void AutomationRuntimeTests::leaseCopiesReleaseOnlyOnce() {
    Automation::AdmissionController controller;
    auto acquired = controller.tryAcquire(true);
    QVERIFY(acquired);
    auto lease = std::move(acquired.get());
    auto copy = lease;
    lease = {};
    QCOMPARE(controller.snapshot().globalInFlight, 1);
    QCOMPARE(controller.snapshot().backgroundTasks, 1);
    copy = {};
    QCOMPARE(controller.snapshot().globalInFlight, 0);
    QCOMPARE(controller.snapshot().backgroundTasks, 0);
}

void AutomationRuntimeTests::backgroundCapacityIsIndependent() {
    Automation::AdmissionLimits limits;
    limits.maximumGlobalInFlight = 4;
    limits.maximumBackgroundTasks = 1;
    Automation::AdmissionController controller(limits);
    const auto first = controller.tryAcquire(true);
    QVERIFY(first);
    const auto second = controller.tryAcquire(true);
    QVERIFY(!second);
    QCOMPARE(second.getError().code, Automation::AutomationErrorCode::Busy);
    QVERIFY(controller.tryAcquire(false));
}

void AutomationRuntimeTests::stoppingRejectsWithoutChangingCounters() {
    Automation::AdmissionController controller;
    const auto existing = controller.tryAcquire();
    QVERIFY(existing);
    controller.setAccepting(false);
    const auto rejected = controller.tryAcquire();
    QVERIFY(!rejected);
    QCOMPARE(rejected.getError().code, Automation::AutomationErrorCode::OperationUnavailable);
    QVERIFY(!controller.snapshot().accepting);
    QCOMPARE(controller.snapshot().globalInFlight, 1);
}

void AutomationRuntimeTests::committedHostGateCannotBeReopened() {
    Automation::CoreRuntime runtime(nullptr, nullptr);
    Automation::AdmissionController controller;
    controller.setAdmissionGate([&runtime] { return !runtime.shutdownCommitted(); });
    auto existing = controller.tryAcquire(true);
    QVERIFY(existing);
    QVERIFY(controller.snapshot().accepting);

    runtime.commitShutdown();
    for (const bool backgroundTask : {false, true}) {
        // Runtime reconfiguration may reopen a listener, but cannot reopen a
        // committed host's admission gate or increment either concurrency counter.
        controller.setAccepting(false);
        controller.setAccepting(true);
        const auto rejected = controller.tryAcquire(backgroundTask);
        QVERIFY(!rejected);
        QCOMPARE(rejected.getError().code, Automation::AutomationErrorCode::OperationUnavailable);
        QVERIFY(!controller.snapshot().accepting);
        QCOMPARE(controller.snapshot().globalInFlight, 1);
        QCOMPARE(controller.snapshot().backgroundTasks, 1);
    }

    auto lease = std::move(existing.get());
    lease = {};
    QCOMPARE(controller.snapshot().globalInFlight, 0);
    QCOMPARE(controller.snapshot().backgroundTasks, 0);
}
