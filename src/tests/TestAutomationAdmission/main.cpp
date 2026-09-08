#include "Automation/Public/AdmissionController.h"

#include <QList>
#include <QtTest>

class TestAutomationAdmission final : public QObject {
    Q_OBJECT

private slots:

    void capacity_data() {
        QTest::addColumn<int>("capacity");
        QTest::newRow("single-request") << 1;
        QTest::newRow("default-capacity") << Automation::AdmissionLimits{}.maximumGlobalInFlight;
    }

    void capacity() {
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

    void leaseCopiesReleaseOnlyOnce() {
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

    void backgroundCapacityIsIndependent() {
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

    void stoppingRejectsWithoutChangingCounters() {
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
};

QTEST_GUILESS_MAIN(TestAutomationAdmission)
#include "main.moc"
