#include "tst_bootstrap.h"
#include "tst_shutdown_probe.h"

#include <QCoreApplication>
#include <QtTest>

int main(int argc, char *argv[]) {
    // The exit-path probes need to own the process from the very first instruction,
    // so they are dispatched before QtTest creates its application object.
    const auto probeExitCode = liteRunShutdownProbe(argc, argv);
    if (probeExitCode >= 0)
        return probeExitCode;

    QCoreApplication application(argc, argv);
    BootstrapTests tests;
    return QTest::qExec(&tests, argc, argv);
}
