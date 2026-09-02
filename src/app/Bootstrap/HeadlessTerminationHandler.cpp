#include "HeadlessTerminationHandler.h"

#include <QSocketNotifier>

#include <cerrno>
#include <cstring>
#include <utility>

#ifdef Q_OS_WIN
#  include <QWinEventNotifier>
#  include <qt_windows.h>

#  include <atomic>
#else
#  include <fcntl.h>
#  include <signal.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

namespace {
#ifdef Q_OS_WIN
    std::atomic<HANDLE> g_consoleEvent = nullptr;
    std::atomic<DWORD> g_consoleEventType = CTRL_C_EVENT;

    BOOL WINAPI consoleControlHandler(const DWORD eventType) {
        if (eventType != CTRL_C_EVENT && eventType != CTRL_BREAK_EVENT)
            return FALSE;

        g_consoleEventType.store(eventType, std::memory_order_relaxed);
        const auto event = g_consoleEvent.load(std::memory_order_acquire);
        return event && SetEvent(event) ? TRUE : FALSE;
    }
#else
    volatile sig_atomic_t g_signalWriteDescriptor = -1;

    void unixSignalHandler(const int signalNumber) {
        const auto savedErrno = errno;
        const auto descriptor = static_cast<int>(g_signalWriteDescriptor);
        if (descriptor >= 0) {
            const auto value = static_cast<unsigned char>(signalNumber);
            (void) ::write(descriptor, &value, sizeof(value));
        }
        errno = savedErrno;
    }

    bool configureSignalDescriptor(const int descriptor, QString &error) {
        const auto statusFlags = ::fcntl(descriptor, F_GETFL, 0);
        if (statusFlags == -1 || ::fcntl(descriptor, F_SETFL, statusFlags | O_NONBLOCK) == -1) {
            const auto errorNumber = errno;
            error = QStringLiteral("Could not make the signal socket non-blocking: %1")
                        .arg(QString::fromLocal8Bit(std::strerror(errorNumber)));
            return false;
        }

        const auto descriptorFlags = ::fcntl(descriptor, F_GETFD, 0);
        if (descriptorFlags == -1 ||
            ::fcntl(descriptor, F_SETFD, descriptorFlags | FD_CLOEXEC) == -1) {
            const auto errorNumber = errno;
            error = QStringLiteral("Could not make the signal socket close-on-exec: %1")
                        .arg(QString::fromLocal8Bit(std::strerror(errorNumber)));
            return false;
        }
        return true;
    }
#endif
}

struct HeadlessTerminationHandler::Private {
    Callback callback;
    bool started = false;
    bool terminating = false;

#ifdef Q_OS_WIN
    HANDLE event = nullptr;
    std::unique_ptr<QWinEventNotifier> notifier;
#else
    int descriptors[2] = {-1, -1};
    struct sigaction previousInterruptAction {};
    struct sigaction previousTerminateAction {};
    bool interruptInstalled = false;
    bool terminateInstalled = false;
    std::unique_ptr<QSocketNotifier> notifier;
#endif
};

QString headlessTerminationSignalName(const HeadlessTerminationSignal signal) {
    switch (signal) {
        case HeadlessTerminationSignal::Interrupt:
#ifdef Q_OS_WIN
            return QStringLiteral("CTRL_C_EVENT");
#else
            return QStringLiteral("SIGINT");
#endif
        case HeadlessTerminationSignal::Terminate:
            return QStringLiteral("SIGTERM");
        case HeadlessTerminationSignal::ConsoleBreak:
            return QStringLiteral("CTRL_BREAK_EVENT");
    }
    return QStringLiteral("unknown termination signal");
}

HeadlessTerminationHandler::HeadlessTerminationHandler() : d(std::make_unique<Private>()) {
}

HeadlessTerminationHandler::~HeadlessTerminationHandler() {
    stop();
}

bool HeadlessTerminationHandler::start(Callback callback, QString &error) {
    error.clear();
    if (d->started)
        return true;
    if (!callback) {
        error = QStringLiteral("A headless termination callback is required");
        return false;
    }

#ifdef Q_OS_WIN
    const auto event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event) {
        error = QStringLiteral("Could not create the console termination event: Windows error %1")
                    .arg(GetLastError());
        return false;
    }

    HANDLE expectedEvent = nullptr;
    if (!g_consoleEvent.compare_exchange_strong(expectedEvent, event, std::memory_order_release,
                                                std::memory_order_relaxed)) {
        CloseHandle(event);
        error = QStringLiteral("A console termination handler is already active");
        return false;
    }
    if (!SetConsoleCtrlHandler(consoleControlHandler, TRUE)) {
        const auto errorNumber = GetLastError();
        g_consoleEvent.store(nullptr, std::memory_order_release);
        CloseHandle(event);
        error = QStringLiteral("Could not install the console termination handler: Windows error %1")
                    .arg(errorNumber);
        return false;
    }

    d->event = event;
    d->notifier = std::make_unique<QWinEventNotifier>(event);
    QObject::connect(d->notifier.get(), &QWinEventNotifier::activated, [this] {
        const auto eventType = g_consoleEventType.load(std::memory_order_relaxed);
        handleSignal(eventType == CTRL_BREAK_EVENT ? HeadlessTerminationSignal::ConsoleBreak
                                                  : HeadlessTerminationSignal::Interrupt);
    });
#else
    if (g_signalWriteDescriptor >= 0) {
        error = QStringLiteral("A process termination handler is already active");
        return false;
    }
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, d->descriptors) == -1) {
        const auto errorNumber = errno;
        error = QStringLiteral("Could not create the signal socket pair: %1")
                    .arg(QString::fromLocal8Bit(std::strerror(errorNumber)));
        return false;
    }
    if (!configureSignalDescriptor(d->descriptors[0], error) ||
        !configureSignalDescriptor(d->descriptors[1], error)) {
        ::close(d->descriptors[0]);
        ::close(d->descriptors[1]);
        d->descriptors[0] = -1;
        d->descriptors[1] = -1;
        return false;
    }

    g_signalWriteDescriptor = d->descriptors[1];
    struct sigaction action {};
    action.sa_handler = unixSignalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    if (::sigaction(SIGINT, &action, &d->previousInterruptAction) == -1) {
        const auto errorNumber = errno;
        g_signalWriteDescriptor = -1;
        ::close(d->descriptors[0]);
        ::close(d->descriptors[1]);
        d->descriptors[0] = -1;
        d->descriptors[1] = -1;
        error = QStringLiteral("Could not install the SIGINT handler: %1")
                    .arg(QString::fromLocal8Bit(std::strerror(errorNumber)));
        return false;
    }
    d->interruptInstalled = true;
    if (::sigaction(SIGTERM, &action, &d->previousTerminateAction) == -1) {
        const auto errorNumber = errno;
        ::sigaction(SIGINT, &d->previousInterruptAction, nullptr);
        d->interruptInstalled = false;
        g_signalWriteDescriptor = -1;
        ::close(d->descriptors[0]);
        ::close(d->descriptors[1]);
        d->descriptors[0] = -1;
        d->descriptors[1] = -1;
        error = QStringLiteral("Could not install the SIGTERM handler: %1")
                    .arg(QString::fromLocal8Bit(std::strerror(errorNumber)));
        return false;
    }
    d->terminateInstalled = true;

    d->notifier =
        std::make_unique<QSocketNotifier>(d->descriptors[0], QSocketNotifier::Read);
    QObject::connect(d->notifier.get(), &QSocketNotifier::activated, [this] {
        int receivedSignal = 0;
        unsigned char buffer[32];
        while (true) {
            const auto count = ::read(d->descriptors[0], buffer, sizeof(buffer));
            if (count > 0) {
                receivedSignal = buffer[count - 1];
                continue;
            }
            if (count == -1 && errno == EINTR)
                continue;
            break;
        }
        if (receivedSignal == SIGINT)
            handleSignal(HeadlessTerminationSignal::Interrupt);
        else if (receivedSignal == SIGTERM)
            handleSignal(HeadlessTerminationSignal::Terminate);
    });
#endif

    d->callback = std::move(callback);
    d->terminating = false;
    d->started = true;
    return true;
}

void HeadlessTerminationHandler::stop() {
    if (!d->started)
        return;

#ifdef Q_OS_WIN
    d->notifier->setEnabled(false);
    d->notifier.reset();
    SetConsoleCtrlHandler(consoleControlHandler, FALSE);
    g_consoleEvent.store(nullptr, std::memory_order_release);
    CloseHandle(d->event);
    d->event = nullptr;
#else
    d->notifier->setEnabled(false);
    d->notifier.reset();
    if (d->terminateInstalled)
        ::sigaction(SIGTERM, &d->previousTerminateAction, nullptr);
    if (d->interruptInstalled)
        ::sigaction(SIGINT, &d->previousInterruptAction, nullptr);
    d->terminateInstalled = false;
    d->interruptInstalled = false;
    g_signalWriteDescriptor = -1;
    ::close(d->descriptors[0]);
    ::close(d->descriptors[1]);
    d->descriptors[0] = -1;
    d->descriptors[1] = -1;
#endif

    d->callback = {};
    d->terminating = false;
    d->started = false;
}

void HeadlessTerminationHandler::handleSignal(const HeadlessTerminationSignal signal) {
    if (!d->started || d->terminating || !d->callback)
        return;
    if (!d->callback(signal))
        return;

    d->terminating = true;
    d->notifier->setEnabled(false);
}
