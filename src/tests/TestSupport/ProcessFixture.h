#ifndef TESTSUPPORT_PROCESSFIXTURE_H
#define TESTSUPPORT_PROCESSFIXTURE_H

#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

#include <functional>
#include <memory>
#include <vector>

namespace TestSupport {
    bool waitUntil(const std::function<bool()> &predicate, int timeoutMilliseconds = 3000);
    void stopProcess(QProcess &process);
    QByteArray readProcessStdout(QProcess &process);
    QByteArray readProcessStderr(QProcess &process);
    void recordProcessMessage(QProcess &process, const char *direction, const QByteArray &message);

    class ProcessFixture final {
    public:
        explicit ProcessFixture(const QString &name);
        ~ProcessFixture();

        bool isValid() const;
        QString path() const;
        QString filePath(const QString &relative) const;
        QString dataDirectory() const;
        QProcessEnvironment environment() const;
        bool writeConfig(const QJsonObject &configuration) const;
        QProcess &process(const QString &name);
        static bool writeWaveFixture(const QString &path);

    private:
        QTemporaryDir m_directory;
        std::vector<std::unique_ptr<QProcess>> m_processes;
    };
}

#endif // TESTSUPPORT_PROCESSFIXTURE_H
