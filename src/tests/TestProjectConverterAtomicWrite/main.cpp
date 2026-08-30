#include <lite/ProjectConverters/DspxProjectConverter.h>
#include <lite/ProjectConverters/MidiConverter.h>
#include <lite/ProjectModel/AppModel/AppModel.h>

#include "Modules/Audio/AudioFilePublisher.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

#include <functional>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {
    int failures = 0;

    void expect(const bool condition, const QString &message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++failures;
    }

    bool writeFile(const QString &path, const QByteArray &data) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
               file.write(data) == data.size();
    }

    QByteArray readFile(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return file.readAll();
    }

    QStringList directoryEntries(const QString &path) {
        return QDir(path).entryList(QDir::AllEntries | QDir::Hidden | QDir::System |
                                        QDir::NoDotAndDotDot,
                                    QDir::Name);
    }

#ifdef Q_OS_WIN
    class ReplacementBlocker final {
    public:
        explicit ReplacementBlocker(const QString &path) {
            m_handle = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
        }

        ~ReplacementBlocker() {
            if (valid())
                CloseHandle(m_handle);
        }

        Q_DISABLE_COPY_MOVE(ReplacementBlocker)

        [[nodiscard]] bool valid() const {
            return m_handle != INVALID_HANDLE_VALUE;
        }

    private:
        HANDLE m_handle = INVALID_HANDLE_VALUE;
    };
#endif

    template <typename Save>
    void verifyAtomicReplacement(const QString &label, const QString &suffix, Save save,
                                 const std::function<bool(const QByteArray &)> &validOutput) {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("project.") + suffix);
        const QByteArray original = QByteArrayLiteral("existing-file-must-survive");
        expect(directory.isValid() && writeFile(path, original),
               label + QStringLiteral(" fixture must be created"));
        if (!directory.isValid() || readFile(path) != original)
            return;

#ifdef Q_OS_WIN
        {
            ReplacementBlocker blocker(path);
            expect(blocker.valid(), label + QStringLiteral(" replacement blocker must open"));
            const auto entriesBefore = directoryEntries(directory.path());
            QString error;
            const auto saved = save(path, error);
            expect(!saved, label + QStringLiteral(" must report a blocked atomic commit"));
            expect(readFile(path) == original,
                   label + QStringLiteral(" failed commit must preserve the existing file"));
            expect(directoryEntries(directory.path()) == entriesBefore,
                   label + QStringLiteral(" failed commit must remove its temporary file"));
        }
#endif

        QString error;
        const auto saved = save(path, error);
        const auto output = readFile(path);
        expect(saved && error.isEmpty(), label + QStringLiteral(" replacement must succeed"));
        expect(output != original && validOutput(output),
               label + QStringLiteral(" replacement must contain a valid new document"));
        expect(directoryEntries(directory.path()) == QStringList{QFileInfo(path).fileName()},
               label + QStringLiteral(" success must leave only the committed target"));
    }

    void testDspxAtomicWrite() {
        AppModel model;
        model.newProject();
        DspxProjectConverter converter;
        verifyAtomicReplacement(
            QStringLiteral("DSPX"), QStringLiteral("dspx"),
            [&converter, &model](const QString &path, QString &error) {
                return converter.save(path, &model, error);
            },
            [](const QByteArray &output) {
                return output.trimmed().startsWith('{') && output.contains("\"content\"");
            });
    }

    void testMidiAtomicWrite() {
        AppModel model;
        model.newProject();
        MidiConverter converter;
        verifyAtomicReplacement(
            QStringLiteral("MIDI"), QStringLiteral("mid"),
            [&converter, &model](const QString &path, QString &error) {
                return converter.save(path, &model, error);
            },
            [](const QByteArray &output) { return output.startsWith("MThd"); });
    }

    void testAudioPublicationRollback() {
        QTemporaryDir directory;
        const auto firstTarget = directory.filePath(QStringLiteral("a.wav"));
        const auto secondTarget = directory.filePath(QStringLiteral("b.wav"));
        const auto firstTemporary = directory.filePath(QStringLiteral("a.exporting"));
        const auto missingTemporary = directory.filePath(QStringLiteral("b.exporting"));
        const auto fixtureReady =
            directory.isValid() && writeFile(firstTarget, QByteArrayLiteral("original-a")) &&
            writeFile(secondTarget, QByteArrayLiteral("original-b")) &&
            writeFile(firstTemporary, QByteArrayLiteral("replacement-a"));
        expect(fixtureReady,
               QStringLiteral("audio publication fixtures must be created"));
        if (!fixtureReady)
            return;

        const auto result = Audio::Internal::publishAudioFiles(
            {
                {firstTarget, firstTemporary},
                {secondTarget, missingTemporary},
            },
            true);
        expect(!result.succeeded() && result.failedTarget == secondTarget,
               QStringLiteral("missing staged audio must fail publication"));
        expect(readFile(firstTarget) == QByteArrayLiteral("original-a") &&
                   readFile(secondTarget) == QByteArrayLiteral("original-b"),
               QStringLiteral("failed separated audio publication must restore every target"));
        expect(directoryEntries(directory.path()) ==
                   QStringList{QFileInfo(firstTarget).fileName(),
                               QFileInfo(secondTarget).fileName()},
               QStringLiteral("audio publication rollback must remove staged and backup files"));
    }

    void testAudioPublicationNoClobber() {
        QTemporaryDir directory;
        const auto firstTarget = directory.filePath(QStringLiteral("a.wav"));
        const auto secondTarget = directory.filePath(QStringLiteral("b.wav"));
        const auto firstTemporary = directory.filePath(QStringLiteral("a.exporting"));
        const auto secondTemporary = directory.filePath(QStringLiteral("b.exporting"));
        const auto fixtureReady =
            directory.isValid() && writeFile(secondTarget, QByteArrayLiteral("external-b")) &&
            writeFile(firstTemporary, QByteArrayLiteral("replacement-a")) &&
            writeFile(secondTemporary, QByteArrayLiteral("replacement-b"));
        expect(fixtureReady,
               QStringLiteral("reject-mode audio publication fixtures must be created"));
        if (!fixtureReady)
            return;

        const auto result = Audio::Internal::publishAudioFiles(
            {
                {firstTarget, firstTemporary},
                {secondTarget, secondTemporary},
            },
            false);
        expect(!result.succeeded() && result.failedTarget == secondTarget,
               QStringLiteral("reject-mode audio publication must fail on a late target"));
        expect(!QFileInfo::exists(firstTarget) &&
                   readFile(secondTarget) == QByteArrayLiteral("external-b"),
               QStringLiteral("reject-mode rollback must preserve external targets and remove "
                              "partial publication"));
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    testDspxAtomicWrite();
    testMidiAtomicWrite();
    testAudioPublicationRollback();
    testAudioPublicationNoClobber();
    if (failures == 0)
        QTextStream(stdout) << "Validated atomic DSPX, MIDI, and audio replacement" << Qt::endl;
    return failures == 0 ? 0 : 1;
}
