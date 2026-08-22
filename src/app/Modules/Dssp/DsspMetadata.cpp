#include "DsspMetadata.h"

#include "Model/AppOptions/AppOptions.h"

#include <lite/SynthrtEngine/SynthrtEngine.h>
#include <lite/Support/JsonUtils.h>

#include <diffsinger/Bank/SingerSnapshot.h>
#include <diffsinger/Session/VoicebankSession.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QHash>
#include <QVersionNumber>

namespace DsspMetadata {

    namespace {
        constexpr auto kArchId = "diffsinger";
        constexpr auto kArchName = "DiffSinger";

        QJsonObject directParam() {
            return QJsonObject{{QStringLiteral("type"), QStringLiteral("DIRECT")}};
        }

        QJsonObject indirectParam(const QStringList &dependsOn) {
            return QJsonObject{
                {QStringLiteral("type"), QStringLiteral("INDIRECT")},
                {QStringLiteral("depends_on"), QJsonArray::fromStringList(dependsOn)},
            };
        }

        QString singerName(const ds::bank::SingerSnapshot &snapshot) {
            const auto name = QString::fromStdString(snapshot.name);
            return name.isEmpty() ? QString::fromStdString(snapshot.ref.singerId) : name;
        }

        // === Raw singer config access ===
        //
        // Avatar / background / demo audio are present in the singer's raw
        // config.json (characters/<id>/config.json) but the voicebank metadata
        // path (SingerManifest / SingerSnapshot / SingerInfo) does not carry
        // them. Read them directly from the singer config file, resolved through
        // desc.json's "contributes.singers" list.
        QString multilingualText(const QJsonValue &value) {
            if (value.isString())
                return value.toString();
            if (value.isObject()) {
                const auto obj = value.toObject();
                const auto defaultText = obj.value(QStringLiteral("_")).toString();
                if (!defaultText.isEmpty())
                    return defaultText;
                for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
                    if (!it.value().toString().isEmpty())
                        return it.value().toString();
                }
            }
            return {};
        }

        QString mimeTypeForFile(const QString &path) {
            const auto suffix = QFileInfo(path).suffix().toLower();
            if (suffix == QStringLiteral("png"))
                return QStringLiteral("image/png");
            if (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg"))
                return QStringLiteral("image/jpeg");
            if (suffix == QStringLiteral("webp"))
                return QStringLiteral("image/webp");
            if (suffix == QStringLiteral("gif"))
                return QStringLiteral("image/gif");
            if (suffix == QStringLiteral("svg"))
                return QStringLiteral("image/svg+xml");
            if (suffix == QStringLiteral("wav"))
                return QStringLiteral("audio/wav");
            if (suffix == QStringLiteral("mp3"))
                return QStringLiteral("audio/mpeg");
            if (suffix == QStringLiteral("ogg"))
                return QStringLiteral("audio/ogg");
            if (suffix == QStringLiteral("flac"))
                return QStringLiteral("audio/flac");
            if (suffix == QStringLiteral("m4a"))
                return QStringLiteral("audio/mp4");
            return QStringLiteral("application/octet-stream");
        }

        QString fileDataUrl(const QString &path) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                return {};
            const auto data = file.readAll();
            if (data.isEmpty())
                return {};
            return QStringLiteral("data:%1;base64,%2")
                .arg(mimeTypeForFile(path), DsspApi::base64Encode(data));
        }

        struct SingerConfigCacheEntry {
            QString configPath;
            QString configDir;
            QJsonObject config;
        };

        QMutex g_singerConfigMutex;
        QHash<QString, SingerConfigCacheEntry> g_singerConfigCache;

        /// Locate and parse the singer's raw config.json. Returns false when
        /// the package layout or config file cannot be read.
        bool loadSingerConfig(const SingerReference &ref, SingerConfigCacheEntry &entry) {
            const auto apiId = encodeSingerId(ref);
            {
                QMutexLocker locker(&g_singerConfigMutex);
                if (const auto it = g_singerConfigCache.constFind(apiId);
                    it != g_singerConfigCache.constEnd()) {
                    entry = it.value();
                    return !entry.config.isEmpty();
                }
            }

            SingerConfigCacheEntry newEntry;
            auto &engine = SynthrtEngine::instance();
            if (engine.sessionReady()) {
                SingerIdentifier identifier;
                identifier.packageId = ref.packageId;
                identifier.packageVersion = QVersionNumber::fromString(ref.version);
                identifier.singerId = ref.singerId;
                const auto packageDir = engine.packageDirectory(identifier);
                if (!packageDir.empty()) {
                    QJsonObject desc;
                    const auto descPath =
                        QString::fromStdString((packageDir / "desc.json").string());
                    if (JsonUtils::load(descPath, desc)) {
                        const auto contributes = desc.value(QStringLiteral("contributes")).toObject();
                        const auto singerPaths =
                            contributes.value(QStringLiteral("singers")).toArray();
                        for (const auto &singerPathValue : singerPaths) {
                            const auto relativePath = singerPathValue.toString();
                            if (relativePath.isEmpty())
                                continue;
                            const auto configPath = QString::fromStdString(
                                (packageDir / relativePath.toStdString()).string());
                            QJsonObject config;
                            if (!JsonUtils::load(configPath, config))
                                continue;
                            if (config.value(QStringLiteral("id")).toString() == ref.singerId) {
                                newEntry.configPath = configPath;
                                newEntry.configDir = QFileInfo(configPath).absolutePath();
                                newEntry.config = config;
                                break;
                            }
                        }
                    }
                }
            }

            {
                QMutexLocker locker(&g_singerConfigMutex);
                g_singerConfigCache.insert(apiId, newEntry);
            }
            entry = std::move(newEntry);
            return !entry.config.isEmpty();
        }

        QString singerResourceUrl(const SingerReference &ref, const QString &field) {
            SingerConfigCacheEntry entry;
            if (!loadSingerConfig(ref, entry))
                return {};
            const auto relativePath =
                multilingualText(entry.config.value(field)).trimmed();
            if (relativePath.isEmpty())
                return {};
            return fileDataUrl(QDir(entry.configDir).filePath(relativePath));
        }

        QJsonObject makeSingerInfo(const ds::bank::SingerSnapshot &snapshot) {
            SingerReference ref;
            ref.packageId = QString::fromStdString(snapshot.ref.packageId);
            ref.version = QString::fromStdString(snapshot.ref.version);
            ref.singerId = QString::fromStdString(snapshot.ref.singerId);

            QStringList speakers;
            for (const auto &id : snapshot.speakerIds)
                speakers.append(QString::fromStdString(id));
            speakers.sort();

            QJsonObject languages;
            for (const auto &language : snapshot.languageInfos) {
                const auto languageId = QString::fromStdString(language.languageId());
                const auto name = QString::fromStdString(language.name());
                const auto defaultLyric = appOptions->general()->defaultLyrics.value(languageId);
                languages.insert(languageId,
                                 QJsonObject{
                                     {QStringLiteral("name"), name},
                                     {QStringLiteral("default_lyric"), defaultLyric},
                                 });
            }

            const auto apiId = encodeSingerId(ref);
            const auto mixGroup = QStringLiteral("%1@%2").arg(ref.packageId, ref.version);
            const auto defaultSpeaker = speakers.isEmpty() ? QString() : speakers.first();

            return QJsonObject{
                {QStringLiteral("id"), apiId},
                {QStringLiteral("name"), singerName(snapshot)},
                {QStringLiteral("arch"), QString::fromLatin1(kArchId)},
                {QStringLiteral("mix_group"), mixGroup},
                {QStringLiteral("languages"), languages},
                {QStringLiteral("default_language"),
                 QString::fromStdString(snapshot.defaultLanguage)},
                {QStringLiteral("arch_specific_info"),
                 QJsonObject{{QStringLiteral("speakers"),
                              QJsonArray::fromStringList(speakers)}}},
                {QStringLiteral("default_extra"),
                 QJsonObject{{QStringLiteral("speaker"), defaultSpeaker}}},
            };
        }
    } // namespace

    bool parseSingerId(const QString &apiId, SingerReference &out) {
        const auto atIndex = apiId.indexOf('@');
        const auto leftBracketIndex = apiId.indexOf('[', atIndex + 1);
        const auto rightBracketIndex = apiId.lastIndexOf(']');
        if (atIndex <= 0 || leftBracketIndex <= atIndex + 1 ||
            rightBracketIndex != apiId.size() - 1 || rightBracketIndex <= leftBracketIndex + 1)
            return false;
        out.packageId = apiId.left(atIndex);
        out.version = apiId.mid(atIndex + 1, leftBracketIndex - atIndex - 1);
        out.singerId = apiId.mid(leftBracketIndex + 1, rightBracketIndex - leftBracketIndex - 1);
        if (out.packageId.isEmpty() || out.version.isEmpty() || out.singerId.isEmpty())
            return false;
        return !QVersionNumber::fromString(out.version).isNull();
    }

    QString encodeSingerId(const SingerReference &ref) {
        if (ref.packageId.isEmpty() || ref.version.isEmpty() || ref.singerId.isEmpty())
            return {};
        return QStringLiteral("%1@%2[%3]").arg(ref.packageId, ref.version, ref.singerId);
    }

    std::shared_ptr<const ds::session::VoicebankSnapshot> currentSnapshot() {
        if (!SynthrtEngine::instance().sessionReady())
            return nullptr;
        return SynthrtEngine::instance().session().snapshot();
    }

    const ds::bank::SingerSnapshot *findSinger(const SingerReference &ref) {
        const auto snapshot = currentSnapshot();
        if (!snapshot)
            return nullptr;
        ds::bank::SingerRef singerRef{ref.packageId.toStdString(), ref.singerId.toStdString(),
                                      ref.version.toStdString()};
        return snapshot->findSinger(singerRef);
    }

    const ds::bank::SingerSnapshot *findSingerByApiId(const QString &apiId, SingerReference &out) {
        if (!parseSingerId(apiId, out))
            return nullptr;
        return findSinger(out);
    }

    DsspApi::Result applicationInfo() {
        return DsspApi::Result::ok(
            QJsonObject{{QStringLiteral("dssp"),
                         QJsonObject{{QStringLiteral("api_version"), 1}}}});
    }

    DsspApi::Result architectureList() {
        const QJsonArray list{
            QJsonObject{
                {QStringLiteral("id"), QString::fromLatin1(kArchId)},
                {QStringLiteral("name"), QString::fromLatin1(kArchName)},
                {QStringLiteral("pronunciation_mode"), QStringLiteral("FULL")},
                {QStringLiteral("phoneme_mode"), QStringLiteral("FULL")},
                {QStringLiteral("parameters"),
                 QJsonObject{
                     {QStringLiteral("expressiveness"), directParam()},
                     {QStringLiteral("pitch"),
                      indirectParam({QStringLiteral("expressiveness")})},
                     {QStringLiteral("breathiness"),
                      indirectParam({QStringLiteral("pitch")})},
                     {QStringLiteral("tension"), indirectParam({QStringLiteral("pitch")})},
                     {QStringLiteral("voicing"), indirectParam({QStringLiteral("pitch")})},
                     {QStringLiteral("energy"), indirectParam({QStringLiteral("pitch")})},
                     {QStringLiteral("mouth_opening"),
                      indirectParam({QStringLiteral("pitch")})},
                     {QStringLiteral("gender"), directParam()},
                     {QStringLiteral("velocity"), directParam()},
                     {QStringLiteral("tone_shift"), directParam()},
                 }},
                {QStringLiteral("audio_dependencies"),
                 QJsonArray{
                     QStringLiteral("pitch"),
                     QStringLiteral("breathiness"),
                     QStringLiteral("tension"),
                     QStringLiteral("voicing"),
                     QStringLiteral("energy"),
                     QStringLiteral("mouth_opening"),
                     QStringLiteral("gender"),
                     QStringLiteral("velocity"),
                     QStringLiteral("tone_shift"),
                 }},
            },
        };
        return DsspApi::Result::ok(list);
    }

    DsspApi::Result architecture(const QString &archId) {
        if (archId != QLatin1String(kArchId))
            return DsspApi::Result::fail(DsspApi::unknownArch(archId));
        const auto list = architectureList();
        return DsspApi::Result::ok(list.body.toArray().first().toObject());
    }

    DsspApi::Result singerList() {
        const auto snapshot = currentSnapshot();
        if (!snapshot)
            return DsspApi::Result::fail(
                DsspApi::internalError(QStringLiteral("Inference engine is not ready")));
        QJsonArray singers;
        for (const auto &singer : snapshot->singers) {
            if (singer.ref.singerId.empty())
                continue;
            singers.append(makeSingerInfo(singer));
        }
        return DsspApi::Result::ok(singers);
    }

    DsspApi::Result archSingerList(const QString &archId) {
        if (archId != QLatin1String(kArchId))
            return DsspApi::Result::fail(DsspApi::unknownArch(archId));
        return singerList();
    }

    DsspApi::Result singer(const QString &singerId) {
        SingerReference ref;
        const auto *snapshot = findSingerByApiId(singerId, ref);
        if (!snapshot)
            return DsspApi::Result::fail(DsspApi::singerNotExist(singerId));
        return DsspApi::Result::ok(makeSingerInfo(*snapshot));
    }

    DsspApi::Result singerAvatar(const QString &singerId) {
        SingerReference ref;
        if (!findSingerByApiId(singerId, ref))
            return DsspApi::Result::fail(DsspApi::singerNotExist(singerId));
        return DsspApi::Result::ok(
            QJsonObject{{QStringLiteral("avatar_url"),
                         singerResourceUrl(ref, QStringLiteral("avatar"))}});
    }

    DsspApi::Result singerBackground(const QString &singerId) {
        SingerReference ref;
        if (!findSingerByApiId(singerId, ref))
            return DsspApi::Result::fail(DsspApi::singerNotExist(singerId));
        return DsspApi::Result::ok(
            QJsonObject{{QStringLiteral("background_url"),
                         singerResourceUrl(ref, QStringLiteral("background"))}});
    }

    DsspApi::Result singerDemoAudioList(const QString &singerId) {
        SingerReference ref;
        if (!findSingerByApiId(singerId, ref))
            return DsspApi::Result::fail(DsspApi::singerNotExist(singerId));
        SingerConfigCacheEntry entry;
        if (!loadSingerConfig(ref, entry))
            return DsspApi::Result::ok(QJsonArray{});
        const auto demoAudio = entry.config.value(QStringLiteral("demoAudio")).toArray();
        QJsonArray result;
        for (const auto &demoValue : demoAudio) {
            if (!demoValue.isObject())
                continue;
            const auto demo = demoValue.toObject();
            const auto relativePath = multilingualText(demo.value(QStringLiteral("path"))).trimmed();
            if (relativePath.isEmpty())
                continue;
            const auto audioUrl = fileDataUrl(QDir(entry.configDir).filePath(relativePath));
            if (audioUrl.isEmpty())
                continue;
            result.append(QJsonObject{
                {QStringLiteral("name"),
                 multilingualText(demo.value(QStringLiteral("name")))},
                {QStringLiteral("audio_url"), audioUrl},
            });
        }
        return DsspApi::Result::ok(result);
    }

    DsspApi::Result envTag(const QJsonObject &body) {
        QJsonObject context;
        if (!DsspApi::readObject(body, QStringLiteral("context"), context)) {
            return DsspApi::Result::fail(DsspApi::validationError(
                QStringLiteral("Missing request field \"context\"")));
        }
        QString arch;
        if (!DsspApi::readString(context, QStringLiteral("arch"), arch)) {
            return DsspApi::Result::fail(DsspApi::validationError(
                QStringLiteral("Missing request field \"context.arch\"")));
        }
        if (arch != QLatin1String(kArchId))
            return DsspApi::Result::fail(DsspApi::unknownArch(arch));

        QJsonArray singers;
        if (!DsspApi::readArray(context, QStringLiteral("singers"), singers)) {
            return DsspApi::Result::fail(DsspApi::validationError(
                QStringLiteral("Missing request field \"context.singers\"")));
        }

        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(QCoreApplication::applicationVersion().toUtf8());
        hash.addData("\0", 1);

        QStringList singerIds;
        for (const auto &singerValue : singers) {
            if (!singerValue.isObject())
                continue;
            const auto singerId = singerValue.toObject().value(QStringLiteral("id")).toString();
            if (!singerId.isEmpty())
                singerIds.append(singerId);
        }
        singerIds.sort();
        for (const auto &singerId : singerIds) {
            SingerReference ref;
            const auto *snapshot = findSingerByApiId(singerId, ref);
            if (!snapshot)
                continue;
            hash.addData(ref.packageId.toUtf8());
            hash.addData("\0", 1);
            hash.addData(ref.version.toUtf8());
            hash.addData("\0", 1);
            hash.addData(ref.singerId.toUtf8());
            hash.addData("\0", 1);
        }

        return DsspApi::Result::ok(QJsonObject{
            {QStringLiteral("env_tag"), QString::fromLatin1(hash.result().toHex())}});
    }

} // namespace DsspMetadata
