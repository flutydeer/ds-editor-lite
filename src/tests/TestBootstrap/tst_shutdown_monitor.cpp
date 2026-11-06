#include "tst_bootstrap.h"
#include "tst_shutdown_probe.h"

#include "Bootstrap/ShutdownMonitor.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QSemaphore>
#include <QTextStream>
#include <QThreadPool>
#include <QTimer>
#include <QtTest>

#include <chrono>
#include <cstring>
#include <memory>
#include <thread>

namespace {
    // The budget the probes arm. It is small on purpose: the test target runs in CI
    // and every case must finish in seconds, not in the production 30 second window.
    constexpr int ProbeBudgetMs = 700;
    constexpr int ProbeWindowMs = 4000;

    // Returns int so a probe can end with `return blockForever();` and keep MSVC's
    // "must return a value" analysis quiet.
    int blockForever() {
        for (;;)
            std::this_thread::sleep_for(std::chrono::seconds(3600));
        return 0;
    }

    int runStalledProbe(int argc, char *argv[]) {
        qputenv("DS_EDITOR_EXIT_FORCE_EXIT_MS", QByteArray::number(ProbeBudgetMs));
        // Reporting is not what this case tests; keep it out of the way.
        qputenv("DS_EDITOR_EXIT_WATCHDOG_MS", "100000");
        const auto application = std::make_unique<QCoreApplication>(argc, argv);
        auto &monitor = ShutdownMonitor::instance();
        monitor.start();
        monitor.commit();
        monitor.setStage(ShutdownMonitor::Stage::ContextTeardown);
        return blockForever();
    }

    int runIdleProbe(int argc, char *argv[]) {
        qputenv("DS_EDITOR_EXIT_FORCE_EXIT_MS", QByteArray::number(ProbeBudgetMs));
        qputenv("DS_EDITOR_EXIT_WATCHDOG_MS", "100000");
        QCoreApplication application(argc, argv);
        // The watchdog runs, but no exit was committed: the process must live longer
        // than the budget and end on its own.
        ShutdownMonitor::instance().start();
        QTimer::singleShot(3 * ProbeBudgetMs, &application, &QCoreApplication::quit);
        return application.exec();
    }

    int runLateTeardownProbe(int argc, char *argv[]) {
        qputenv("DS_EDITOR_EXIT_FORCE_EXIT_MS", QByteArray::number(ProbeBudgetMs));
        qputenv("DS_EDITOR_EXIT_WATCHDOG_MS", "100");
        auto application = std::make_unique<QCoreApplication>(argc, argv);
        auto &monitor = ShutdownMonitor::instance();
        monitor.start();
        monitor.commit();
        // The global pool is destroyed by the Qt application teardown and waits for
        // its runnables without a deadline. That is the phase a watchdog stopped
        // before returning from main() used to lose.
        QSemaphore entered;
        QSemaphore released;
        QThreadPool::globalInstance()->start([&entered, &released] {
            entered.release();
            released.acquire();
        });
        entered.acquire();
        application.reset();
        return 0;
    }

    int runBlockedLogProbe(int argc, char *argv[]) {
        qputenv("DS_EDITOR_EXIT_FORCE_EXIT_MS", QByteArray::number(ProbeBudgetMs));
        // Reporting starts early, so the reporter really does try to log while the
        // handler below is blocked.
        qputenv("DS_EDITOR_EXIT_WATCHDOG_MS", "100");
        const auto application = std::make_unique<QCoreApplication>(argc, argv);
        auto &monitor = ShutdownMonitor::instance();
        monitor.start();

        // Mirrors the production handler: the mutex is held across the write and the
        // flush, so a thread stuck there keeps every other writer out.
        static QMutex logMutex;
        qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &message) {
            const QMutexLocker locker(&logMutex);
            QTextStream(stderr) << message << Qt::endl;
        });
        QSemaphore mutexHeld;
        std::thread([&mutexHeld] {
            const QMutexLocker locker(&logMutex);
            mutexHeld.release();
            blockForever();
        }).detach();
        mutexHeld.acquire();

        // The commit is pure atomics, so this post-commit log is what blocks inside the
        // handler. The budget must keep running from the commit regardless.
        monitor.commit();
        qInfo() << "post-commit log with the log mutex held";
        return blockForever();
    }

    int runAbsoluteBudgetProbe(int argc, char *argv[]) {
        qputenv("DS_EDITOR_EXIT_FORCE_EXIT_MS", QByteArray::number(ProbeBudgetMs));
        qputenv("DS_EDITOR_EXIT_WATCHDOG_MS", "100000");
        const auto application = std::make_unique<QCoreApplication>(argc, argv);
        auto &monitor = ShutdownMonitor::instance();
        monitor.start();
        monitor.commit();
        // A shutdown that keeps reaching new stages must not extend the budget: the
        // deadline counts from the commit, never from the last stage change.
        const auto stages = {ShutdownMonitor::Stage::CloseRequested,
                             ShutdownMonitor::Stage::WaitingForBackgroundTasks,
                             ShutdownMonitor::Stage::BackgroundTasksDone,
                             ShutdownMonitor::Stage::ClosingWindow};
        for (;;) {
            for (const auto stage : stages) {
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
                monitor.setStage(stage);
            }
        }
        return blockForever();
    }

    int runDisabledProbe(int argc, char *argv[]) {
        // Zero disables the only hard exit the process has, so the process has to be
        // reclaimed by the parent instead of by an in-process fallback.
        qputenv("DS_EDITOR_EXIT_FORCE_EXIT_MS", "0");
        qputenv("DS_EDITOR_EXIT_WATCHDOG_MS", "100000");
        const auto application = std::make_unique<QCoreApplication>(argc, argv);
        auto &monitor = ShutdownMonitor::instance();
        monitor.start();
        monitor.commit();
        monitor.setStage(ShutdownMonitor::Stage::ContextTeardown);
        return blockForever();
    }

    int runAboutToQuitOrderProbe(int argc, char *argv[]) {
        qputenv("DS_EDITOR_EXIT_FORCE_EXIT_MS", QByteArray::number(ProbeBudgetMs));
        qputenv("DS_EDITOR_EXIT_WATCHDOG_MS", "100000");
        QCoreApplication application(argc, argv);
        ShutdownMonitor::instance().start();
        // The composition-root pattern: the commit fallback is connected before the
        // subsystem callbacks that can block, so arming cannot be delayed by them.
        QObject::connect(&application, &QCoreApplication::aboutToQuit, &application,
                         [] { ShutdownMonitor::instance().commit(); });
        QObject::connect(&application, &QCoreApplication::aboutToQuit, &application,
                         [] { blockForever(); });
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
        return application.exec();
    }

    struct ProbeOutcome {
        bool exited = false;
        qint64 elapsedMs = 0;
        QProcess::ExitStatus exitStatus = QProcess::NormalExit;
        int exitCode = -1;
        QString output;
    };

    // Runs one scenario as a child process. The parent waits for a bounded window and
    // reclaims the child itself when the scenario is supposed to stay alive.
    ProbeOutcome runShutdownProbe(const QString &scenario, const int windowMs) {
        ProbeOutcome outcome;
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        QElapsedTimer timer;
        timer.start();
        process.start(QCoreApplication::applicationFilePath(),
                      {QStringLiteral("--shutdown-probe"), scenario});
        if (!process.waitForStarted(10000)) {
            outcome.output = QStringLiteral("probe did not start");
            return outcome;
        }
        outcome.exited = process.waitForFinished(windowMs);
        outcome.elapsedMs = timer.elapsed();
        if (outcome.exited) {
            outcome.exitStatus = process.exitStatus();
            outcome.exitCode = process.exitCode();
        } else {
            process.kill();
            process.waitForFinished(5000);
        }
        outcome.output = QString::fromLocal8Bit(process.readAllStandardOutput());
        return outcome;
    }

    QString describe(const ProbeOutcome &outcome) {
        return QStringLiteral("exited=%1 after %2 ms, code=%3\n%4")
            .arg(outcome.exited ? QStringLiteral("yes") : QStringLiteral("no"))
            .arg(outcome.elapsedMs)
            .arg(outcome.exitCode)
            .arg(outcome.output);
    }

    // Every hard-exit assertion shares the same shape: the child ends itself, and the
    // exit code is a failure because a stalled shutdown is not a successful exit.
    void verifyWatchdogEnded(const ProbeOutcome &outcome) {
        QVERIFY2(outcome.exited,
                 qPrintable(QStringLiteral("the probe never exited: ") + describe(outcome)));
        QVERIFY2(outcome.exitStatus == QProcess::NormalExit,
                 qPrintable(QStringLiteral("the probe crashed instead of exiting: ") +
                            describe(outcome)));
        QVERIFY2(outcome.exitCode == 1,
                 qPrintable(QStringLiteral("unexpected exit code: ") + describe(outcome)));
    }
}

int liteRunShutdownProbe(int argc, char *argv[]) {
    if (argc < 3 || std::strcmp(argv[1], "--shutdown-probe") != 0)
        return -1;
    const auto scenario = QString::fromLocal8Bit(argv[2]);
    if (scenario == QStringLiteral("stalled"))
        return runStalledProbe(argc, argv);
    if (scenario == QStringLiteral("idle"))
        return runIdleProbe(argc, argv);
    if (scenario == QStringLiteral("late-teardown"))
        return runLateTeardownProbe(argc, argv);
    if (scenario == QStringLiteral("blocked-log"))
        return runBlockedLogProbe(argc, argv);
    if (scenario == QStringLiteral("absolute-budget"))
        return runAbsoluteBudgetProbe(argc, argv);
    if (scenario == QStringLiteral("disabled"))
        return runDisabledProbe(argc, argv);
    if (scenario == QStringLiteral("about-to-quit-order"))
        return runAboutToQuitOrderProbe(argc, argv);
    return -1;
}

void BootstrapTests::watchdogBoundsStalledExit() {
    const auto outcome = runShutdownProbe(QStringLiteral("stalled"), ProbeWindowMs);
    verifyWatchdogEnded(outcome);
    // The budget counts from the commit, so the exit cannot come earlier than that.
    QVERIFY2(outcome.elapsedMs >= ProbeBudgetMs / 2,
             qPrintable(QStringLiteral("exit came too early: ") + describe(outcome)));
}

void BootstrapTests::watchdogIgnoresIdleProcess() {
    const auto outcome = runShutdownProbe(QStringLiteral("idle"), ProbeWindowMs);
    QVERIFY2(outcome.exited,
             qPrintable(QStringLiteral("the idle probe never exited: ") + describe(outcome)));
    // Without a committed exit there is no budget: the process ends on its own.
    QVERIFY2(outcome.exitCode == 0,
             qPrintable(QStringLiteral("unexpected exit code: ") + describe(outcome)));
    QVERIFY2(outcome.elapsedMs >= 2 * ProbeBudgetMs,
             qPrintable(QStringLiteral("the idle probe exited too early: ") + describe(outcome)));
}

void BootstrapTests::watchdogCoversLateQtTeardown() {
    verifyWatchdogEnded(runShutdownProbe(QStringLiteral("late-teardown"), ProbeWindowMs));
}

void BootstrapTests::watchdogExitsWithoutLogging() {
    // The child is blocked inside a post-commit log call while the watchdog fires: the
    // hard exit must not need the logging mutex.
    verifyWatchdogEnded(runShutdownProbe(QStringLiteral("blocked-log"), ProbeWindowMs));
}

void BootstrapTests::watchdogUsesOneAbsoluteBudget() {
    const auto outcome = runShutdownProbe(QStringLiteral("absolute-budget"), ProbeWindowMs);
    verifyWatchdogEnded(outcome);
    QVERIFY2(outcome.elapsedMs < 3 * ProbeBudgetMs,
             qPrintable(QStringLiteral("stage changes pushed the exit out: ") + describe(outcome)));
}

void BootstrapTests::disabledHardExitIsLeftToTheParent() {
    // With DS_EDITOR_EXIT_FORCE_EXIT_MS=0 the process must never end itself, so this
    // case reclaims the child from the outside instead of trusting it to exit.
    const auto outcome = runShutdownProbe(QStringLiteral("disabled"), 2 * ProbeBudgetMs);
    QVERIFY2(!outcome.exited, qPrintable(QStringLiteral("the probe exited although the hard "
                                                        "exit was disabled: ") +
                                         describe(outcome)));
}

void BootstrapTests::aboutToQuitFallbackCommitsBeforeBlockingCallbacks() {
    // The commit fallback runs before the blocking callback, so the budget covers the
    // stall. This is the ordering the composition root installs.
    verifyWatchdogEnded(runShutdownProbe(QStringLiteral("about-to-quit-order"), ProbeWindowMs));
}
