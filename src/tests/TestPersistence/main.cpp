#include "Model/AppOptions/AppOptions.h"
#include "Utils/FileRevision.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>

namespace {
    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    bool writeFile(const QString &path, const QByteArray &data) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
               file.write(data) == data.size();
    }

    bool verifyFileRevision() {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("revision.txt"));
        bool ok =
            expect(writeFile(path, QByteArrayLiteral("aaaa")), "revision fixture must be written");
        FileRevision revision;
        ok &= expect(FileRevisionUtils::capture(path, revision), "file revision must be captured");
        ok &= expect(FileRevisionUtils::matchesCurrent(path, revision),
                     "unchanged file must match its revision");
        ok &= expect(writeFile(path, QByteArrayLiteral("bbbb")),
                     "same-size external change must be written");
        ok &= expect(!FileRevisionUtils::matchesCurrent(path, revision),
                     "content hash must detect same-size external changes");
        return ok;
    }

    bool verifyAppOptionsPersistence() {
        QTemporaryDir directory;
        bool ok = expect(directory.isValid(), "temporary config directory must exist");
        AppOptions options(directory.path());
        const auto path = options.configPath();
        ok &= expect(!QFile::exists(path), "first launch must not create a config file");
        ok &= expect(!options.general()->defaultSingingLanguage.isEmpty(),
                     "missing config must retain option defaults");

        options.general()->uiLanguage = QStringLiteral("en_US");
        ok &= expect(options.saveAndNotify(AppOptionsGlobal::General),
                     "explicit option save must succeed");
        ok &= expect(QFile::exists(path), "explicit option save must create the config file");

        const QByteArray externalConfig = QJsonDocument(QJsonObject{
                                                            {QStringLiteral("external"), true},
        })
                                              .toJson();
        ok &= expect(writeFile(path, externalConfig), "external config change must be written");
        options.general()->uiLanguage = QStringLiteral("zh_CN");
        ok &= expect(!options.saveAndNotify(AppOptionsGlobal::General),
                     "stale config snapshot must refuse overwrite");
        QFile file(path);
        ok &= expect(file.open(QIODevice::ReadOnly) && file.readAll() == externalConfig,
                     "rejected config save must preserve external contents");
        return ok;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    bool ok = true;
    ok &= verifyFileRevision();
    ok &= verifyAppOptionsPersistence();
    return ok ? 0 : 1;
}
