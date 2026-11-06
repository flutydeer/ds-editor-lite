#ifndef BOOTSTRAP_SHUTDOWNMONITOR_H
#define BOOTSTRAP_SHUTDOWNMONITOR_H

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <system_error>
#include <thread>

#if defined(Q_OS_WIN)
#  include <qt_windows.h>
#  include <tlhelp32.h>
#endif

// Watches the whole process lifetime from the exit decision to the end of the
// process.
//
// Closing the editor is a chain of waits: the window waits for the background
// task pool, then the automation server is stopped, then the application context
// drains its subsystems, and finally Qt tears down its global thread pool and
// static objects. Every one of those waits is unbounded, and a single thread that
// never returns leaves the "exiting" dialog on screen forever. Those hangs are
// rare and timing dependent, so this monitor records the shutdown stage the host
// has reached and, once the host commits the exit, bounds the process with one
// absolute budget.
//
// Two properties are deliberate:
//
//   * The watchdog thread is created during normal startup. Creating a thread is
//     itself an operation that can fail or block on a busy machine, so it must not
//     be the first thing a stalled shutdown path does.
//   * Once committed, the protection lasts until the operating system reclaims the
//     process. There is no stop or join entry: the late Qt and static teardown is
//     exactly the phase that needs it. A static instance whose destructor joined
//     the thread would remove the protection it exists to provide, so the state is
//     leaked on purpose and the process-lifetime thread ends with the process.
//
// The monitor is header-only on purpose: the editor and the exit-path subprocess
// tests in TestBootstrap share this exact implementation, and a few dozen lines do
// not justify a library target of their own.
//
// Environment variables:
//   DS_EDITOR_EXIT_WATCHDOG_MS       stall report threshold after the commit
//   DS_EDITOR_EXIT_FORCE_EXIT_MS     absolute budget after the commit; 0 disables
//                                    the only hard exit this process has
//   DS_EDITOR_SHUTDOWN_TIMEOUT_MS    per-phase wait threshold for runtime stops,
//                                    not an exit policy (see the design document)
class ShutdownMonitor {
public:
    enum class Stage {
        Idle,
        CloseRequested,
        WaitingForBackgroundTasks,
        BackgroundTasksDone,
        ClosingWindow,
        AutomationStopping,
        AutomationStopped,
        ContextTeardown,
        ContextTasksDrained,
        ContextTeardownDone,
    };

    // The state object lives as long as the process. See the note above about the
    // deliberate leak: a destructor here would join the watchdog during static
    // teardown and drop the protection for the final phase.
    static ShutdownMonitor &instance() {
        static ShutdownMonitor *monitor = new ShutdownMonitor();
        return *monitor;
    }

    ShutdownMonitor(const ShutdownMonitor &) = delete;
    ShutdownMonitor &operator=(const ShutdownMonitor &) = delete;

    // Starts the watchdog and the diagnostics thread while the process is still
    // healthy. Repeated calls are ignored. Targets that never call it (unit tests
    // outside the exit probes) simply have no watchdog, which is also why commit()
    // must stay usable without it.
    void start() {
        if (m_started.exchange(true))
            return;
        m_reportAfterMs = positiveEnvironmentValue("DS_EDITOR_EXIT_WATCHDOG_MS", 10000);
        m_forceExitMs = forcedExitDeadlineMs();
        auto watchdogStarted = false;
        try {
            m_thread = std::thread([this] { watch(); });
            watchdogStarted = true;
        } catch (const std::system_error &error) {
            // Thread creation is not guaranteed. Losing the watchdog is reported, but
            // it must not take the whole editor down during startup.
            qWarning().noquote()
                << QStringLiteral("[shutdown] no exit watchdog, its thread could not be created "
                                  "(%1); the process has no hard exit")
                       .arg(QString::fromLocal8Bit(error.what()));
        }
        try {
            // The diagnostics thread is separate on purpose: reports format strings,
            // enumerate threads and take the logging mutex. A failure here costs the
            // stall reports only, the exit budget above still runs.
            m_reporter = std::thread([this] { reportLoop(); });
        } catch (const std::system_error &error) {
            qWarning().noquote()
                << QStringLiteral("[shutdown] no stall reports, the diagnostics thread could not "
                                  "be created (%1)")
                       .arg(QString::fromLocal8Bit(error.what()));
        }
        if (!watchdogStarted)
            return;
        qInfo().noquote() << QStringLiteral(
                                 "[shutdown] watchdog started, reports stalls over %1 ms from the "
                                 "exit commit (budget: %2)")
                                 .arg(m_reportAfterMs)
                                 .arg(m_forceExitMs > 0 ? QStringLiteral("%1 ms").arg(m_forceExitMs)
                                                        : QStringLiteral("disabled"));
    }

    // Commits the formal exit. Later calls are ignored, so every participant of the
    // shutdown sequence can commit without coordinating with the others. This is
    // deliberately pure atomic work: the host freezes the runtime right after, and
    // only then logs, so neither the fence nor the budget can be delayed by a log
    // call that blocks on a stuck process.
    void commit() {
        if (m_committed.exchange(true, std::memory_order_acq_rel))
            return;
        m_commitNanos.store(nowNanos(), std::memory_order_release);
    }

    [[nodiscard]] bool committed() const {
        return m_committed.load(std::memory_order_acquire);
    }

    // Records the stage the host has reached. Stage records only matter once the
    // exit is committed, so the log line is written from that point on.
    void setStage(const Stage stage) {
        if (m_stage.exchange(stage, std::memory_order_acq_rel) == stage)
            return;
        if (!committed())
            return;
        qInfo().noquote() << QStringLiteral("[shutdown] stage %1 (+%2 ms)")
                                 .arg(stageName(stage))
                                 .arg(elapsedMs());
    }

    // Threshold for a runtime stop handshake that has a deadline of its own (the
    // automation server stop). It never decides whether the process keeps living:
    // the exit budget above is the single hard exit.
    static int shutdownTimeoutMs() {
        return positiveEnvironmentValue("DS_EDITOR_SHUTDOWN_TIMEOUT_MS", 30000);
    }

private:
    ShutdownMonitor() = default;

    static qint64 nowNanos() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    static int positiveEnvironmentValue(const char *name, const int fallback) {
        bool ok = false;
        const auto value = qEnvironmentVariableIntValue(name, &ok);
        return ok && value > 0 ? static_cast<int>(value) : fallback;
    }

    // Hard-exit budget. A runnable that ignores termination also blocks
    // QThreadPool's destructor, which runs long after every bounded wait has given
    // up, and the dialogue decision behind us means nothing is left to save. A
    // process that cannot finish its shutdown therefore leaves instead of hanging
    // until the user kills it. Zero disables this, and with it every hard exit the
    // editor has.
    static int forcedExitDeadlineMs() {
        bool ok = false;
        const auto value = qEnvironmentVariableIntValue("DS_EDITOR_EXIT_FORCE_EXIT_MS", &ok);
        if (!ok)
            return 30000;
        return value > 0 ? static_cast<int>(value) : 0;
    }

    static QString stageName(const Stage stage) {
        switch (stage) {
            case Stage::Idle:
                return QStringLiteral("idle");
            case Stage::CloseRequested:
                return QStringLiteral("close-requested");
            case Stage::WaitingForBackgroundTasks:
                return QStringLiteral("waiting-for-background-tasks");
            case Stage::BackgroundTasksDone:
                return QStringLiteral("background-tasks-done");
            case Stage::ClosingWindow:
                return QStringLiteral("closing-window");
            case Stage::AutomationStopping:
                return QStringLiteral("automation-stopping");
            case Stage::AutomationStopped:
                return QStringLiteral("automation-stopped");
            case Stage::ContextTeardown:
                return QStringLiteral("context-teardown");
            case Stage::ContextTasksDrained:
                return QStringLiteral("context-tasks-drained");
            case Stage::ContextTeardownDone:
                return QStringLiteral("context-teardown-done");
        }
        return QStringLiteral("unknown");
    }

    // A long-lived process easily owns dozens of threads, so the report keeps a
    // bounded slice and states how many entries were left out.
    static QString summarizeEntries(const QStringList &entries) {
        constexpr qsizetype maximumEntries = 48;
        if (entries.size() <= maximumEntries)
            return entries.join(QStringLiteral(", "));
        return QStringLiteral("%1, +%2 more")
            .arg(entries.mid(0, maximumEntries).join(QStringLiteral(", ")))
            .arg(entries.size() - maximumEntries);
    }

    // Best-effort thread listing. Thread names alone already tell whether the
    // editor is waiting for the task pool, the automation server or a plugin
    // worker, so platforms without enumeration fall back to the stage alone.
    static QString threadSummary() {
#if defined(Q_OS_LINUX)
        const auto root = QStringLiteral("/proc/self/task");
        const auto tids = QDir(root).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        QStringList entries;
        entries.reserve(tids.size());
        for (const auto &tid : tids) {
            QFile nameFile(root + QLatin1Char('/') + tid + QStringLiteral("/comm"));
            auto name = nameFile.open(QIODevice::ReadOnly)
                            ? QString::fromUtf8(nameFile.readAll()).trimmed()
                            : QStringLiteral("?");
            if (name.isEmpty())
                name = QStringLiteral("?");
            QFile statusFile(root + QLatin1Char('/') + tid + QStringLiteral("/status"));
            auto state = QStringLiteral("?");
            if (statusFile.open(QIODevice::ReadOnly)) {
                const auto lines = QString::fromUtf8(statusFile.readAll()).split(QLatin1Char('\n'));
                for (const auto &line : lines) {
                    if (!line.startsWith(QStringLiteral("State:")))
                        continue;
                    const auto fields = line.simplified().split(QLatin1Char(' '));
                    if (fields.size() > 1)
                        state = fields.at(1);
                    break;
                }
            }
            entries.append(QStringLiteral("%1:%2:%3").arg(tid, name, state));
        }
        return QStringLiteral(" (tid:name:state = %1)").arg(summarizeEntries(entries));
#elif defined(Q_OS_WIN)
        const auto processId = GetCurrentProcessId();
        const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return {};
        QStringList entries;
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        if (Thread32First(snapshot, &entry)) {
            do {
                if (entry.th32OwnerProcessID != processId)
                    continue;
                auto name = QStringLiteral("?");
                const auto thread =
                    OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ThreadID);
                if (thread) {
                    PWSTR description = nullptr;
                    if (SUCCEEDED(GetThreadDescription(thread, &description)) && description) {
                        name = QString::fromWCharArray(description);
                        LocalFree(description);
                    }
                    CloseHandle(thread);
                }
                entries.append(QStringLiteral("%1:%2").arg(entry.th32ThreadID).arg(name));
            } while (Thread32Next(snapshot, &entry));
        }
        CloseHandle(snapshot);
        return QStringLiteral(" (tid:name = %1)").arg(summarizeEntries(entries));
#else
        return {};
#endif
    }

    qint64 elapsedMs() const {
        const auto committedNanos = m_commitNanos.load(std::memory_order_acquire);
        if (committedNanos == 0)
            return 0;
        return (nowNanos() - committedNanos) / 1000000;
    }

    // Diagnostics are a separate channel by construction. Formatting, thread
    // enumeration and the logging mutex can each block on a stuck process, and a
    // blocked reporter must not be able to delay the hard exit. The watchdog hands
    // over elapsed time and stage through atomics and never waits for this thread.
    void reportLoop() {
        using namespace std::chrono_literals;
        quint64 handled = 0;
        while (true) {
            std::this_thread::sleep_for(100ms);
            const auto requested = m_reportSequence.load(std::memory_order_acquire);
            if (requested == handled)
                continue;
            handled = requested;
            const auto elapsed = m_reportElapsedMs.load(std::memory_order_relaxed);
            const auto stage = m_reportStage.load(std::memory_order_relaxed);
            qWarning().noquote() << QStringLiteral(
                                        "[shutdown] still exiting after %1 ms, stuck at stage %2%3")
                                        .arg(elapsed)
                                        .arg(stageName(stage), threadSummary());
        }
    }

    void watch() {
        using namespace std::chrono_literals;
        constexpr int maximumReports = 60;
        // Read once: from here on the loop only reads the commit state and the
        // clock, so nothing the shutdown path does can change the decision.
        const auto forceExitMs = m_forceExitMs;
        const auto reportAfterMs = m_reportAfterMs;
        int reports = 0;
        auto lastReport = std::chrono::steady_clock::now();
        while (true) {
            std::this_thread::sleep_for(200ms);
            // An editor can run for hours without ever committing an exit.
            if (!m_committed.load(std::memory_order_acquire))
                continue;
            const auto elapsed = elapsedMs();
            // The one hard exit of the process. It reads the clock and two
            // pre-initialised values, and then ends the process: no Qt call, no log,
            // no thread enumeration, no allocation and no helper thread, because any
            // of those can block on a process that is already stuck.
            if (forceExitMs > 0 && elapsed >= forceExitMs)
                std::_Exit(EXIT_FAILURE);
            if (elapsed < reportAfterMs || reports >= maximumReports)
                continue;
            const auto now = std::chrono::steady_clock::now();
            if (reports > 0 && now - lastReport < 10s)
                continue;
            lastReport = now;
            ++reports;
            // Hand-off only. Everything that can allocate, block or fail happens on
            // the diagnostics thread, so this branch cannot keep the watchdog away
            // from the deadline above.
            m_reportElapsedMs.store(elapsed, std::memory_order_relaxed);
            m_reportStage.store(m_stage.load(std::memory_order_acquire), std::memory_order_relaxed);
            m_reportSequence.fetch_add(1, std::memory_order_release);
        }
    }

    std::atomic_bool m_started = false;
    std::atomic_bool m_committed = false;
    std::atomic<Stage> m_stage = Stage::Idle;
    std::atomic<qint64> m_commitNanos = 0;
    std::atomic<quint64> m_reportSequence = 0;
    std::atomic<qint64> m_reportElapsedMs = 0;
    std::atomic<Stage> m_reportStage = Stage::Idle;
    int m_reportAfterMs = 10000;
    int m_forceExitMs = 30000;
    std::thread m_thread;
    std::thread m_reporter;
};

#endif // BOOTSTRAP_SHUTDOWNMONITOR_H
