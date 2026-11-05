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
#include <thread>

#if defined(Q_OS_WIN)
#  include <qt_windows.h>
#  include <tlhelp32.h>
#endif

// Watches the editor's shutdown sequence.
//
// Closing the editor is a chain of waits: the window waits for the background
// task pool, then the automation server is stopped, then the application
// context drains its subsystems. A single thread that never returns keeps the
// "exiting" dialog on screen forever and leaves the user no choice but killing
// the process. Those hangs are rare and timing dependent, so this monitor
// records the shutdown stage the main thread has reached and, once a deadline
// passes, keeps reporting the stuck stage from a watchdog thread. The log then
// names the stage instead of ending in silence.
//
// The monitor is header-only on purpose: automation sources are compiled into
// several targets (the editor, the protocol tests), and every one of them needs
// the same stage log without a new library target.
//
// Timeouts are in milliseconds and can be overridden with
// DS_EDITOR_SHUTDOWN_TIMEOUT_MS (waits for background threads) and
// DS_EDITOR_EXIT_WATCHDOG_MS (watchdog deadline).
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
        Finished,
    };

    static ShutdownMonitor &instance() {
        static ShutdownMonitor monitor;
        return monitor;
    }

    ShutdownMonitor(const ShutdownMonitor &) = delete;
    ShutdownMonitor &operator=(const ShutdownMonitor &) = delete;

    // Arms the watchdog once. Later calls are ignored, so any participant of the
    // shutdown sequence can arm it without coordinating with the others.
    void begin() {
        if (m_armed.exchange(true))
            return;
        m_stage.store(Stage::Idle, std::memory_order_release);
        m_deadlineMs = watchdogDeadlineMs();
        m_started = std::chrono::steady_clock::now();
        m_stop.store(false, std::memory_order_release);
        m_thread = std::thread([this] { watch(); });
        qInfo().noquote() << QStringLiteral("[shutdown] watchdog armed, reports stalls over %1 ms")
                                 .arg(m_deadlineMs);
    }

    void setStage(const Stage stage) {
        if (!m_armed.load(std::memory_order_acquire))
            return;
        if (m_stage.exchange(stage, std::memory_order_acq_rel) == stage)
            return;
        qInfo().noquote() << QStringLiteral("[shutdown] stage %1 (+%2 ms)")
                                 .arg(stageName(stage))
                                 .arg(elapsedMs());
    }

    // Stops the watchdog and reports the total shutdown time.
    void finish() {
        if (!m_armed.exchange(false))
            return;
        m_stage.store(Stage::Finished, std::memory_order_release);
        m_stop.store(true, std::memory_order_release);
        if (m_thread.joinable())
            m_thread.join();
        qInfo().noquote() << QStringLiteral("[shutdown] complete in %1 ms").arg(elapsedMs());
    }

    ~ShutdownMonitor() {
        m_stop.store(true, std::memory_order_release);
        if (m_thread.joinable())
            m_thread.join();
    }

    // Shared timeout for waiting on background threads during shutdown. A task
    // that ignores termination must not keep the editor alive forever.
    static int shutdownTimeoutMs() {
        return positiveEnvironmentValue("DS_EDITOR_SHUTDOWN_TIMEOUT_MS", 30000);
    }

    // A shutdown wait gave up while a background thread still owns objects that
    // the teardown is about to destroy. There is no safe way to continue: the
    // abandoned thread would dereference freed state, and leaking it only moves
    // the crash. Ending the process now is the only bounded and safe outcome.
    [[noreturn]] static void exitWithRunningThreads(const char *reason) {
        terminateNow(QStringLiteral("[shutdown] %1; ending the process instead of tearing down "
                                    "state the background thread still uses%2")
                         .arg(QString::fromLatin1(reason), threadSummary()));
    }

private:
    ShutdownMonitor() = default;

    // The record is best effort, the exit is not. The logging handler holds its
    // mutex across the write and flush, and a thread that is stuck there because the
    // output pipe is full would otherwise keep this call from ever reaching _Exit,
    // so the record is written from a helper thread and given a bounded window.
    [[noreturn]] static void terminateNow(const QString &message) {
        reportWithoutBlocking(message);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::_Exit(EXIT_FAILURE);
    }

    // Diagnostics must never stall the watchdog itself, for the same reason.
    static void reportWithoutBlocking(const QString &message) {
        std::thread([message] { qWarning().noquote() << message; }).detach();
    }

    static int positiveEnvironmentValue(const char *name, const int fallback) {
        bool ok = false;
        const auto value = qEnvironmentVariableIntValue(name, &ok);
        return ok && value > 0 ? static_cast<int>(value) : fallback;
    }

    static int watchdogDeadlineMs() {
        return positiveEnvironmentValue("DS_EDITOR_EXIT_WATCHDOG_MS", 10000);
    }

    // Hard-exit deadline. A runnable that ignores termination also blocks
    // QThreadPool's destructor, which runs long after every bounded wait here
    // has given up, so no in-process wait can bound the exit on its own. Once
    // the document decision has been made nothing is left to save, therefore a
    // process that cannot finish its shutdown leaves instead of hanging until
    // the user kills it. Set the variable to 0 to disable the escape hatch.
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
            case Stage::Finished:
                return QStringLiteral("finished");
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
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - m_started)
            .count();
    }

    void watch() {
        using namespace std::chrono_literals;
        constexpr int maximumReports = 60;
        const auto forceExitMs = forcedExitDeadlineMs();
        int reports = 0;
        auto lastReport = std::chrono::steady_clock::now();
        while (!m_stop.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(200ms);
            if (m_stop.load(std::memory_order_acquire))
                break;
            const auto stage = m_stage.load(std::memory_order_acquire);
            if (stage == Stage::Finished)
                continue;
            const auto elapsed = elapsedMs();
            if (forceExitMs > 0 && elapsed >= forceExitMs) {
                terminateNow(
                    QStringLiteral("[shutdown] still stuck after %1 ms, forcing the process to "
                                   "exit at stage %2%3")
                        .arg(elapsed)
                        .arg(stageName(stage), threadSummary()));
            }
            if (elapsed < m_deadlineMs || reports >= maximumReports)
                continue;
            const auto now = std::chrono::steady_clock::now();
            if (reports > 0 && now - lastReport < 10s)
                continue;
            lastReport = now;
            ++reports;
            reportWithoutBlocking(
                QStringLiteral("[shutdown] still exiting after %1 ms, stuck at stage %2%3")
                    .arg(elapsed)
                    .arg(stageName(stage), threadSummary()));
        }
    }

    std::atomic_bool m_armed = false;
    std::atomic_bool m_stop = false;
    std::atomic<Stage> m_stage = Stage::Idle;
    std::chrono::steady_clock::time_point m_started;
    int m_deadlineMs = 10000;
    std::thread m_thread;
};

#endif // BOOTSTRAP_SHUTDOWNMONITOR_H
