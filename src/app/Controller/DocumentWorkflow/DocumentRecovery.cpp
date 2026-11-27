#include "DocumentRecovery.h"

#include "Bootstrap/AppDataPaths.h"
#include "Modules/ProjectConverters/DspxProjectConverterUi.h"

#include <QJsonDocument>
#include <QSaveFile>

namespace DocumentRecovery {
    static QDir directory() {
        return QDir(QDir(AppDataPaths::applicationData()).filePath(QStringLiteral("recovery")));
    }

    QList<Record> pending() {
        const auto dir = directory();
        QList<Record> result;
        for (const auto &file : dir.entryInfoList({QStringLiteral("*.dspx")}, QDir::Files)) {
            const auto key = file.completeBaseName();
            QFile info(dir.filePath(key + ".json"));
            if (!info.open(QIODevice::ReadOnly))
                continue;
            const auto object = QJsonDocument::fromJson(info.readAll()).object();
            if (object.contains("path") && object.contains("name"))
                result.append(
                    {key, object.value("path").toString(), object.value("name").toString()});
        }
        return result;
    }

    bool write(const QString &key, const QString &path, const QString &name, const AppModel *model,
               const LoopSettings &loop, QString &error) {
        const auto dir = directory();
        if (!dir.mkpath(QStringLiteral("."))) {
            error = dir.absolutePath();
            return false;
        }
        const auto metaPath = dir.filePath(key + ".json");
        if (!QFile::exists(metaPath)) {
            QSaveFile meta(metaPath);
            const auto bytes = QJsonDocument(QJsonObject{
                                                 {"path", path},
                                                 {"name", name}
            })
                                   .toJson(QJsonDocument::Compact);
            if (!meta.open(QIODevice::WriteOnly) || meta.write(bytes) != bytes.size() ||
                !meta.commit()) {
                error = meta.errorString();
                return false;
            }
        }
        const auto snapshot = dir.filePath(key + ".dspx");
        DspxProjectConverterUi converter(loop);
        return converter.saveSnapshot(snapshot, path.isEmpty() ? snapshot : path, model, error);
    }

    bool read(const Record &record, Automation::DocumentDraftDto &draft, QString &error) {
        DspxProjectConverterUi converter;
        AppModel model;
        if (!converter.load(directory().filePath(record.file + ".dspx"), &model, error,
                            IProjectConverter::NewProject))
            return false;
        auto data = model.takeProjectData();
        data.setAudioReferenceDirectory(record.path.isEmpty()
                                            ? directory().absolutePath()
                                            : QFileInfo(record.path).absolutePath());
        draft = Automation::documentDraftDto(data, converter.loadedLoopSettings());
        return true;
    }

    void remove(const QString &key) {
        if (key.isEmpty())
            return;
        const auto dir = directory();
        QFile::remove(dir.filePath(key + ".dspx"));
        QFile::remove(dir.filePath(key + ".json"));
    }
}
