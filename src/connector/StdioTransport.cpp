#include "StdioTransport.h"

#include <QElapsedTimer>
#include <QMetaObject>
#include <QMutex>
#include <QQueue>
#include <QThread>
#include <QWaitCondition>

#include <algorithm>
#include <limits>

#ifdef Q_OS_WIN
#  include <qt_windows.h>
#else
#  include <cerrno>
#  include <fcntl.h>
#  include <unistd.h>
#endif

namespace DsConnector {
    namespace {
        constexpr qsizetype MaxStdioMessageBytes = 16 * 1024 * 1024;
        constexpr qsizetype MaxQueuedInputFrames = 64;
        constexpr qsizetype MaxQueuedInputBytes = 32 * 1024 * 1024;
        constexpr qsizetype MaxQueuedOutputFrames = 64;
        constexpr qsizetype MaxQueuedOutputBytes = 32 * 1024 * 1024;
        constexpr qsizetype OutputWriteChunkBytes = 4 * 1024;
        constexpr auto InputDrainBatchSize = 16;
        constexpr auto OutputWriteStallTimeoutMs = 5000;
        constexpr auto WriterStartTimeoutMs = 5000;
        constexpr auto ThreadStopTimeoutMs = 1000;
    }

    struct StdioTransportState final {
        QMutex mutex;
        QWaitCondition inputNotFull;
        QWaitCondition outputAvailable;
        QWaitCondition writerStarted;
        QQueue<QByteArray> inputFrames;
        QQueue<QByteArray> outputFrames;
        qsizetype inputBytes = 0;
        qsizetype outputOutstandingFrames = 0;
        qsizetype outputOutstandingBytes = 0;
        bool inputWakePending = false;
        bool inputTerminal = false;
        bool inputTerminalHandled = false;
        QString inputError;
        bool outputClosing = false;
        bool writerReady = false;
        QString writerStartupError;
        bool stopping = false;
    };

    class StdioReaderThread final : public QThread {
        Q_OBJECT

    public:
        explicit StdioReaderThread(std::shared_ptr<StdioTransportState> state,
                                  QObject *parent = nullptr)
            : QThread(parent), m_state(std::move(state)) {
        }

    signals:
        void inputAvailable();

    protected:
        void run() override {
            QByteArray pending;
            QByteArray chunk(64 * 1024, Qt::Uninitialized);
#ifdef Q_OS_WIN
            const auto input = GetStdHandle(STD_INPUT_HANDLE);
            if (input == nullptr || input == INVALID_HANDLE_VALUE) {
                finish(QStringLiteral("Unable to read stdin"));
                return;
            }
#endif
            while (!isInterruptionRequested()) {
                qint64 count = 0;
#ifdef Q_OS_WIN
                DWORD bytesRead = 0;
                const auto succeeded =
                    ReadFile(input, chunk.data(), static_cast<DWORD>(chunk.size()), &bytesRead,
                             nullptr);
                if (!succeeded && GetLastError() != ERROR_BROKEN_PIPE) {
                    finish(QStringLiteral("Unable to read stdin"));
                    return;
                }
                count = succeeded ? static_cast<qint64>(bytesRead) : 0;
#else
                const auto bytesRead = ::read(STDIN_FILENO, chunk.data(), chunk.size());
                if (bytesRead < 0 && errno == EINTR)
                    continue;
                if (bytesRead < 0) {
                    finish(QStringLiteral("Unable to read stdin"));
                    return;
                }
                count = bytesRead;
#endif
                if (count == 0) {
                    const auto payloadSize =
                        pending.endsWith('\r') ? pending.size() - 1 : pending.size();
                    if (payloadSize > MaxStdioMessageBytes) {
                        finish(QStringLiteral("frame_too_large"));
                        return;
                    }
                    if (!pending.isEmpty() && !enqueue(std::move(pending)))
                        return;
                    finish({});
                    return;
                }

                pending.append(chunk.constData(), count);
                while (true) {
                    const auto newline = pending.indexOf('\n');
                    if (newline < 0)
                        break;
                    auto line = pending.first(newline);
                    pending.remove(0, newline + 1);
                    const auto payloadSize = line.endsWith('\r') ? line.size() - 1 : line.size();
                    if (payloadSize > MaxStdioMessageBytes) {
                        finish(QStringLiteral("frame_too_large"));
                        return;
                    }
                    if (!line.isEmpty() && !enqueue(std::move(line)))
                        return;
                }
                const auto payloadSize =
                    pending.endsWith('\r') ? pending.size() - 1 : pending.size();
                if (payloadSize > MaxStdioMessageBytes) {
                    finish(QStringLiteral("frame_too_large"));
                    return;
                }
            }
        }

    private:
        bool enqueue(QByteArray line) {
            bool notify = false;
            {
                QMutexLocker locker(&m_state->mutex);
                while (!m_state->stopping &&
                       (m_state->inputFrames.size() >= MaxQueuedInputFrames ||
                        m_state->inputBytes + line.size() > MaxQueuedInputBytes)) {
                    m_state->inputNotFull.wait(&m_state->mutex);
                }
                if (m_state->stopping)
                    return false;
                m_state->inputBytes += line.size();
                m_state->inputFrames.enqueue(std::move(line));
                if (!m_state->inputWakePending) {
                    m_state->inputWakePending = true;
                    notify = true;
                }
            }
            if (notify)
                emit inputAvailable();
            return true;
        }

        void finish(QString error) {
            bool notify = false;
            {
                const QMutexLocker locker(&m_state->mutex);
                if (m_state->stopping || m_state->inputTerminal)
                    return;
                m_state->inputTerminal = true;
                m_state->inputError = std::move(error);
                if (!m_state->inputWakePending) {
                    m_state->inputWakePending = true;
                    notify = true;
                }
            }
            if (notify)
                emit inputAvailable();
        }

        std::shared_ptr<StdioTransportState> m_state;
    };

    class StdioWriterThread final : public QThread {
        Q_OBJECT

    public:
        explicit StdioWriterThread(std::shared_ptr<StdioTransportState> state,
                                  QObject *parent = nullptr)
            : QThread(parent), m_state(std::move(state)) {
        }

    signals:
        void outputDrained();
        void outputFailed(const QString &error);

    protected:
        void run() override {
            QString startupError;
            if (!configureOutput(startupError)) {
                markStarted(std::move(startupError));
                return;
            }
            markStarted({});

            while (!isInterruptionRequested()) {
                QByteArray frame;
                {
                    QMutexLocker locker(&m_state->mutex);
                    while (!m_state->stopping && m_state->outputFrames.isEmpty() &&
                           !m_state->outputClosing) {
                        m_state->outputAvailable.wait(&m_state->mutex);
                    }
                    if (m_state->stopping)
                        return;
                    if (m_state->outputFrames.isEmpty() && m_state->outputClosing) {
                        locker.unlock();
                        emit outputDrained();
                        return;
                    }
                    frame = m_state->outputFrames.dequeue();
                }

                QString writeError;
                if (!writeFrame(frame, writeError)) {
                    if (!writeError.isEmpty())
                        emit outputFailed(writeError);
                    return;
                }
                {
                    const QMutexLocker locker(&m_state->mutex);
                    --m_state->outputOutstandingFrames;
                    m_state->outputOutstandingBytes -= frame.size();
                }
            }
        }

    private:
        void markStarted(QString error) {
            const QMutexLocker locker(&m_state->mutex);
            m_state->writerStartupError = std::move(error);
            m_state->writerReady = m_state->writerStartupError.isEmpty();
            m_state->writerStarted.wakeAll();
        }

        bool configureOutput(QString &error) {
#ifdef Q_OS_WIN
            m_output = GetStdHandle(STD_OUTPUT_HANDLE);
            if (m_output == nullptr || m_output == INVALID_HANDLE_VALUE) {
                error = QStringLiteral("Unable to open stdout");
                return false;
            }
            if (GetFileType(m_output) == FILE_TYPE_PIPE) {
                DWORD mode = PIPE_NOWAIT;
                if (!SetNamedPipeHandleState(m_output, &mode, nullptr, nullptr)) {
                    error = QStringLiteral("Unable to configure stdout backpressure");
                    return false;
                }
                m_nonBlocking = true;
            }
#else
            const auto flags = fcntl(STDOUT_FILENO, F_GETFL, 0);
            if (flags < 0 || fcntl(STDOUT_FILENO, F_SETFL, flags | O_NONBLOCK) < 0) {
                error = QStringLiteral("Unable to configure stdout backpressure");
                return false;
            }
#endif
            return true;
        }

        bool writeFrame(const QByteArray &frame, QString &error) {
            qsizetype offset = 0;
            QElapsedTimer stalled;
            stalled.start();
            while (offset < frame.size()) {
                {
                    const QMutexLocker locker(&m_state->mutex);
                    if (m_state->stopping)
                        return false;
                }
                if (isInterruptionRequested())
                    return false;

                qint64 written = 0;
                bool wouldBlock = false;
#ifdef Q_OS_WIN
                DWORD bytesWritten = 0;
                const auto remaining = static_cast<DWORD>(std::min<qsizetype>(
                    frame.size() - offset,
                    std::min<qsizetype>(OutputWriteChunkBytes,
                                        std::numeric_limits<DWORD>::max())));
                const auto succeeded = WriteFile(m_output, frame.constData() + offset, remaining,
                                                 &bytesWritten, nullptr);
                if (succeeded) {
                    written = static_cast<qint64>(bytesWritten);
                    wouldBlock = m_nonBlocking && bytesWritten == 0;
                } else {
                    const auto code = GetLastError();
                    wouldBlock = m_nonBlocking &&
                                 (code == ERROR_NO_DATA || code == ERROR_PIPE_BUSY);
                    if (!wouldBlock) {
                        error = QStringLiteral("Unable to write MCP response to stdout");
                        return false;
                    }
                }
#else
                const auto remaining =
                    std::min<qsizetype>(frame.size() - offset, OutputWriteChunkBytes);
                const auto result = ::write(STDOUT_FILENO, frame.constData() + offset, remaining);
                if (result > 0) {
                    written = result;
                } else if (result < 0 && errno == EINTR) {
                    continue;
                } else if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    wouldBlock = true;
                } else {
                    error = QStringLiteral("Unable to write MCP response to stdout");
                    return false;
                }
#endif
                if (written > 0) {
                    offset += written;
                    stalled.restart();
                    continue;
                }
                if (!wouldBlock && written == 0) {
                    error = QStringLiteral("Unable to write MCP response to stdout");
                    return false;
                }
                if (stalled.elapsed() >= OutputWriteStallTimeoutMs) {
                    error = QStringLiteral("stdout_write_timeout");
                    return false;
                }
                QThread::msleep(5);
            }
            return true;
        }

        std::shared_ptr<StdioTransportState> m_state;
#ifdef Q_OS_WIN
        HANDLE m_output = INVALID_HANDLE_VALUE;
        bool m_nonBlocking = false;
#endif
    };

    StdioTransport::StdioTransport(QObject *parent)
        : QObject(parent), m_state(std::make_shared<StdioTransportState>()),
          m_reader(new StdioReaderThread(m_state, this)),
          m_writer(new StdioWriterThread(m_state, this)) {
        auto *reader = static_cast<StdioReaderThread *>(m_reader);
        auto *writer = static_cast<StdioWriterThread *>(m_writer);
        connect(reader, &StdioReaderThread::inputAvailable, this, &StdioTransport::drainInput,
                Qt::QueuedConnection);
        connect(writer, &StdioWriterThread::outputDrained, this, &StdioTransport::writerDrained,
                Qt::QueuedConnection);
        connect(writer, &StdioWriterThread::outputFailed, this, &StdioTransport::writerFailed,
                Qt::QueuedConnection);
    }

    StdioTransport::~StdioTransport() {
        stopThreads();
    }

    bool StdioTransport::start(QString *error) {
        if (error)
            error->clear();
        if (m_started) {
            if (error)
                *error = QStringLiteral("stdio transport is already running");
            return false;
        }

        m_writer->start();
        {
            QMutexLocker locker(&m_state->mutex);
            if (!m_state->writerReady && m_state->writerStartupError.isEmpty())
                m_state->writerStarted.wait(&m_state->mutex, WriterStartTimeoutMs);
            if (!m_state->writerReady) {
                if (error) {
                    *error = m_state->writerStartupError.isEmpty()
                                 ? QStringLiteral("Timed out while starting stdout writer")
                                 : m_state->writerStartupError;
                }
                m_state->stopping = true;
                m_state->outputAvailable.wakeAll();
                locker.unlock();
                m_writer->requestInterruption();
                m_writer->wait(ThreadStopTimeoutMs);
                return false;
            }
        }

        m_started = true;
        m_reader->start();
        return true;
    }

    void StdioTransport::writeLine(const QByteArray &line) {
        if (!m_started || m_terminalDelivered)
            return;
        auto framed = line;
        if (!framed.endsWith('\n'))
            framed.append('\n');
        const auto payloadSize = framed.size() - 1;
        QString queueError;
        {
            const QMutexLocker locker(&m_state->mutex);
            if (m_state->stopping || m_state->outputClosing)
                return;
            if (payloadSize > MaxStdioMessageBytes) {
                queueError = QStringLiteral("stdout_frame_too_large");
            } else if (m_state->outputOutstandingFrames >= MaxQueuedOutputFrames ||
                       m_state->outputOutstandingBytes + framed.size() >
                           MaxQueuedOutputBytes) {
                queueError = QStringLiteral("stdout_backpressure_limit_exceeded");
            } else {
                ++m_state->outputOutstandingFrames;
                m_state->outputOutstandingBytes += framed.size();
                m_state->outputFrames.enqueue(std::move(framed));
                m_state->outputAvailable.wakeOne();
            }
        }
        if (!queueError.isEmpty())
            fail(queueError);
    }

    void StdioTransport::drainInput() {
        for (auto count = 0; count < InputDrainBatchSize; ++count) {
            QByteArray line;
            bool terminal = false;
            {
                QMutexLocker locker(&m_state->mutex);
                if (!m_state->inputFrames.isEmpty()) {
                    line = m_state->inputFrames.dequeue();
                    m_state->inputBytes -= line.size();
                    m_state->inputNotFull.wakeOne();
                } else {
                    if (m_state->inputTerminal && !m_state->inputTerminalHandled) {
                        m_state->inputTerminalHandled = true;
                        m_inputTerminalError = m_state->inputError;
                        terminal = true;
                    }
                    m_state->inputWakePending = false;
                }
            }
            if (!line.isEmpty()) {
                emit lineReceived(line);
                continue;
            }
            if (terminal) {
                const QMutexLocker locker(&m_state->mutex);
                m_state->outputClosing = true;
                m_state->outputAvailable.wakeOne();
            }
            return;
        }

        bool scheduleNext = false;
        {
            const QMutexLocker locker(&m_state->mutex);
            scheduleNext = !m_state->inputFrames.isEmpty() ||
                           (m_state->inputTerminal && !m_state->inputTerminalHandled);
            if (!scheduleNext)
                m_state->inputWakePending = false;
        }
        if (scheduleNext)
            QMetaObject::invokeMethod(this, &StdioTransport::drainInput, Qt::QueuedConnection);
    }

    void StdioTransport::writerDrained() {
        if (m_terminalDelivered)
            return;
        m_terminalDelivered = true;
        if (m_inputTerminalError.isEmpty())
            emit inputClosed();
        else
            emit transportError(m_inputTerminalError);
    }

    void StdioTransport::writerFailed(const QString &error) {
        fail(error);
    }

    void StdioTransport::fail(const QString &error) {
        if (m_terminalDelivered)
            return;
        m_terminalDelivered = true;
        {
            const QMutexLocker locker(&m_state->mutex);
            m_state->stopping = true;
            m_state->inputNotFull.wakeAll();
            m_state->outputAvailable.wakeAll();
        }
        m_reader->requestInterruption();
        m_writer->requestInterruption();
        emit transportError(error);
    }

    void StdioTransport::stopThreads() {
        {
            const QMutexLocker locker(&m_state->mutex);
            m_state->stopping = true;
            m_state->inputNotFull.wakeAll();
            m_state->outputAvailable.wakeAll();
            m_state->writerStarted.wakeAll();
        }
        m_reader->requestInterruption();
        m_writer->requestInterruption();
        for (auto *thread : {m_reader, m_writer}) {
            if (!thread->isRunning())
                continue;
            if (!thread->wait(ThreadStopTimeoutMs)) {
                thread->terminate();
                thread->wait();
            }
        }
    }

}

#include "StdioTransport.moc"
