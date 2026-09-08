#include "ProcessFixture.h"

#include <lite/ProductMetadata.h>

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>
#include <QtTest>

namespace TestSupport {
    namespace {
        QString directoryTemplate(const QString &name) {
            auto root = qEnvironmentVariable("DSEL_TEST_ARTIFACTS");
            if (root.isEmpty())
                root = QDir::tempPath();
            QDir().mkpath(root);
            return QDir(root).absoluteFilePath(name + QStringLiteral("-XXXXXX"));
        }

        void appendLog(QProcess &process, const QString &suffix, const QByteArray &bytes) {
            if (bytes.isEmpty())
                return;
            const auto prefix = process.property("testLogPrefix").toString();
            if (prefix.isEmpty())
                return;
            QFile file(prefix + suffix);
            if (file.open(QIODevice::WriteOnly | QIODevice::Append))
                file.write(bytes);
        }
    }

    bool waitUntil(const std::function<bool()> &predicate, const int timeoutMilliseconds) {
        QElapsedTimer timer;
        timer.start();
        while (!predicate() && timer.elapsed() < timeoutMilliseconds) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QThread::msleep(5);
        }
        return predicate();
    }

    void stopProcess(QProcess &process) {
        if (process.state() == QProcess::NotRunning)
            return;
        process.terminate();
        if (!process.waitForFinished(5000)) {
            process.kill();
            process.waitForFinished(5000);
        }
    }

    QByteArray readProcessStdout(QProcess &process) {
        const auto bytes = process.readAllStandardOutput();
        appendLog(process, QStringLiteral(".stdout.log"), bytes);
        return bytes;
    }

    QByteArray readProcessStderr(QProcess &process) {
        const auto bytes = process.readAllStandardError();
        appendLog(process, QStringLiteral(".stderr.log"), bytes);
        return bytes;
    }

    void recordProcessMessage(QProcess &process, const char *direction, const QByteArray &message) {
        appendLog(process, QStringLiteral(".protocol.log"),
                  QByteArray(direction) + ' ' + message + '\n');
    }

    ProcessFixture::ProcessFixture(const QString &name) : m_directory(directoryTemplate(name)) {
        if (m_directory.isValid()) {
            QDir().mkpath(dataDirectory());
            QDir().mkpath(filePath(QStringLiteral("logs")));
            QDir().mkpath(filePath(QStringLiteral("Local")));
        }
    }

    ProcessFixture::~ProcessFixture() {
        QJsonArray processes;
        for (const auto &process : m_processes) {
            stopProcess(*process);
            readProcessStdout(*process);
            readProcessStderr(*process);
            processes.append(QJsonObject{
                {QStringLiteral("program"),     process->program()                              },
                {QStringLiteral("arguments"),   QJsonArray::fromStringList(process->arguments())},
                {QStringLiteral("exit_code"),   process->exitCode()                             },
                {QStringLiteral("exit_status"), int(process->exitStatus())                      },
            });
        }
        if (QTest::currentTestFailed()) {
            m_directory.setAutoRemove(false);
            QFile metadata(filePath(QStringLiteral("processes.json")));
            if (metadata.open(QIODevice::WriteOnly))
                metadata.write(QJsonDocument(processes).toJson());
            qWarning().noquote() << "Process test artifacts:" << path();
        }
    }

    bool ProcessFixture::isValid() const {
        return m_directory.isValid();
    }

    QString ProcessFixture::path() const {
        return m_directory.path();
    }

    QString ProcessFixture::filePath(const QString &relative) const {
        return m_directory.filePath(relative);
    }

    QString ProcessFixture::dataDirectory() const {
        return QDir(path()).filePath(
            QStringLiteral("%1/%2").arg(QString::fromLatin1(LiteProductMetadata::Publisher),
                                        QString::fromLatin1(LiteProductMetadata::ProductName)));
    }

    QProcessEnvironment ProcessFixture::environment() const {
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("DSEL_TEST_DATA_ROOT"), path());
        environment.insert(QStringLiteral("APPDATA"), path());
        environment.insert(QStringLiteral("LOCALAPPDATA"), filePath(QStringLiteral("Local")));
        environment.insert(QStringLiteral("QT_LOGGING_TO_CONSOLE"), QStringLiteral("1"));
        return environment;
    }

    bool ProcessFixture::writeConfig(const QJsonObject &configuration) const {
        QFile file(QDir(dataDirectory()).filePath(QStringLiteral("appConfig.json")));
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
               file.write(QJsonDocument(configuration).toJson()) >= 0;
    }

    QProcess &ProcessFixture::process(const QString &name) {
        auto process = std::make_unique<QProcess>();
        process->setProcessEnvironment(environment());
        process->setProperty("testLogPrefix", filePath(QStringLiteral("logs/") + name));
        m_processes.push_back(std::move(process));
        return *m_processes.back();
    }

    bool ProcessFixture::writeWaveFixture(const QString &path) {
        constexpr quint32 sampleRate = 8000;
        constexpr quint16 channels = 1;
        constexpr quint16 bitsPerSample = 16;
        const QByteArray samples(1600, '\0');
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream.writeRawData("RIFF", 4);
        stream << quint32(36 + samples.size());
        stream.writeRawData("WAVEfmt ", 8);
        stream << quint32(16) << quint16(1) << channels << sampleRate
               << quint32(sampleRate * channels * (bitsPerSample / 8))
               << quint16(channels * (bitsPerSample / 8)) << bitsPerSample;
        stream.writeRawData("data", 4);
        stream << quint32(samples.size());
        stream.writeRawData(samples.constData(), samples.size());
        return stream.status() == QDataStream::Ok;
    }
}
