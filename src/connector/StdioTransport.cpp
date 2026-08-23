#include "StdioTransport.h"

#include <QThread>

#include <cstdio>

#ifdef Q_OS_WIN
#  include <qt_windows.h>
#else
#  include <cerrno>
#  include <unistd.h>
#endif

namespace DsConnector {
    namespace {
        constexpr qsizetype MaxStdioMessageBytes = 16 * 1024 * 1024;
    }

    class StdioReaderThread final : public QThread {
        Q_OBJECT

    public:
        using QThread::QThread;

    signals:
        void lineRead(const QByteArray &line);
        void closed();
        void failed(const QString &error);

    protected:
        void run() override {
            QByteArray pending;
            QByteArray chunk(64 * 1024, Qt::Uninitialized);
            while (!isInterruptionRequested()) {
                qint64 count = 0;
#ifdef Q_OS_WIN
                DWORD bytesRead = 0;
                const auto handle = GetStdHandle(STD_INPUT_HANDLE);
                const auto succeeded =
                    ReadFile(handle, chunk.data(), static_cast<DWORD>(chunk.size()), &bytesRead,
                             nullptr);
                if (!succeeded && GetLastError() != ERROR_BROKEN_PIPE) {
                    emit failed(QStringLiteral("Unable to read stdin"));
                    return;
                }
                count = succeeded ? static_cast<qint64>(bytesRead) : 0;
#else
                const auto bytesRead = ::read(STDIN_FILENO, chunk.data(), chunk.size());
                if (bytesRead < 0 && errno == EINTR)
                    continue;
                if (bytesRead < 0) {
                    emit failed(QStringLiteral("Unable to read stdin"));
                    return;
                }
                count = bytesRead;
#endif
                if (count == 0) {
                    const auto payloadSize =
                        pending.endsWith('\r') ? pending.size() - 1 : pending.size();
                    if (payloadSize > MaxStdioMessageBytes) {
                        emit failed(QStringLiteral("frame_too_large"));
                        return;
                    }
                    if (!pending.isEmpty())
                        emit lineRead(pending);
                    emit closed();
                    return;
                }
                pending.append(chunk.constData(), count);
                while (true) {
                    const auto newline = pending.indexOf('\n');
                    if (newline < 0)
                        break;
                    const auto line = pending.first(newline);
                    pending.remove(0, newline + 1);
                    const auto payloadSize = line.endsWith('\r') ? line.size() - 1 : line.size();
                    if (payloadSize > MaxStdioMessageBytes) {
                        emit failed(QStringLiteral("frame_too_large"));
                        return;
                    }
                    if (!line.isEmpty())
                        emit lineRead(line);
                }
                const auto payloadSize =
                    pending.endsWith('\r') ? pending.size() - 1 : pending.size();
                if (payloadSize > MaxStdioMessageBytes) {
                    emit failed(QStringLiteral("frame_too_large"));
                    return;
                }
            }
        }
    };

    StdioTransport::StdioTransport(QObject *parent)
        : QObject(parent), m_reader(new StdioReaderThread(this)) {
        auto *reader = static_cast<StdioReaderThread *>(m_reader);
        connect(reader, &StdioReaderThread::lineRead, this, &StdioTransport::lineReceived,
                Qt::QueuedConnection);
        connect(reader, &StdioReaderThread::closed, this, &StdioTransport::inputClosed,
                Qt::QueuedConnection);
        connect(reader, &StdioReaderThread::failed, this, &StdioTransport::transportError,
                Qt::QueuedConnection);
    }

    StdioTransport::~StdioTransport() {
        if (m_reader->isRunning()) {
            m_reader->requestInterruption();
            if (!m_reader->wait(100)) {
                m_reader->terminate();
                m_reader->wait();
            }
        }
    }

    bool StdioTransport::start(QString *error) {
        if (error)
            error->clear();
        if (!m_output.open(stdout, QIODevice::WriteOnly, QFileDevice::DontCloseHandle)) {
            if (error)
                *error = QStringLiteral("Unable to open stdout");
            return false;
        }
        m_reader->start();
        return true;
    }

    void StdioTransport::writeLine(const QByteArray &line) {
        if (!m_output.isOpen())
            return;
        auto framed = line;
        if (!framed.endsWith('\n'))
            framed.append('\n');
        if (m_output.write(framed) != framed.size() || !m_output.flush())
            emit transportError(QStringLiteral("Unable to write MCP response to stdout"));
    }

}

#include "StdioTransport.moc"
