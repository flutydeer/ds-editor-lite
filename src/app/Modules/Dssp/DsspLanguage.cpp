#include "DsspLanguage.h"

#include "DsspMetadata.h"

#include <lite/SynthrtEngine/SynthrtEngine.h>
#include <lite/Support/VersionUtils.h>

#include <diffsinger/Session/VoicebankSession.h>
#include <synthrt/G2P/Base/LangCommon.h>

#include <QLoggingCategory>
#include <QVersionNumber>

#include <mutex>

Q_LOGGING_CATEGORY(logDsspLanguage, "dssp.language")

namespace DsspLanguage {

    namespace {

        std::string toUtf8(const QString &value) {
            const auto bytes = value.toUtf8();
            return {bytes.constData(), static_cast<size_t>(bytes.size())};
        }

        QString fromUtf8(const std::string &value) {
            return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
        }

        /// Serializes G2P/S2P access to the shared LanguageService.
        ///
        /// The voicebank session's language conversion runs ONNX sessions that
        /// are not safe for concurrent use. The in-app G2P/S2P tasks are
        /// serialized by their task queues, so concurrent HTTP requests must
        /// be serialized here to avoid racing the shared language models.
        std::mutex g_languageConversionMutex;

        /// Resolve the single-singer synthesis context.
        /// On success returns the resolved singer reference. Otherwise an empty
        /// optional carrying a problem.
        std::optional<DsspApi::Problem> resolveSingleSingerContext(
            const QJsonObject &body, DsspMetadata::SingerReference &singerRef) {
            QJsonObject context;
            if (!DsspApi::readObject(body, QStringLiteral("context"), context)) {
                return DsspApi::validationError(
                    QStringLiteral("Missing request field \"context\""));
            }
            QString arch;
            if (!DsspApi::readString(context, QStringLiteral("arch"), arch)) {
                return DsspApi::validationError(
                    QStringLiteral("Missing request field \"context.arch\""));
            }
            if (arch != QLatin1String("diffsinger"))
                return DsspApi::unknownArch(arch);

            QJsonObject singer;
            if (!DsspApi::readObject(context, QStringLiteral("singer"), singer)) {
                return DsspApi::validationError(
                    QStringLiteral("Missing request field \"context.singer\""));
            }
            QString singerId;
            if (!DsspApi::readString(singer, QStringLiteral("id"), singerId)) {
                return DsspApi::singerConfigInvalid(
                    QStringLiteral("Missing request field \"context.singer.id\""));
            }
            if (!DsspMetadata::parseSingerId(singerId, singerRef)) {
                return DsspApi::singerNotExist(singerId);
            }
            if (!DsspMetadata::findSinger(singerRef)) {
                return DsspApi::singerNotExist(singerId);
            }
            return std::nullopt;
        }

        bool isSkippedLyric(const QString &lyric) {
            if (lyric == QStringLiteral("SP") || lyric == QStringLiteral("AP") ||
                lyric == QStringLiteral("-"))
                return true;
            for (const auto &ch : lyric) {
                if (ch != QChar('+'))
                    return false;
            }
            return true;
        }

        ds::bank::SingerRef toSingerRef(const DsspMetadata::SingerReference &ref) {
            return {ref.packageId.toStdString(), ref.singerId.toStdString(),
                    ref.version.toStdString()};
        }

    } // namespace

    DsspApi::Result pronunciation(const QJsonObject &body) {
        if (!SynthrtEngine::instance().sessionReady()) {
            return DsspApi::Result::fail(
                DsspApi::internalError(QStringLiteral("Inference engine is not ready")));
        }

        DsspMetadata::SingerReference singerRef;
        if (auto problem = resolveSingleSingerContext(body, singerRef))
            return DsspApi::Result::fail(*problem);

        QJsonObject input;
        if (!DsspApi::readObject(body, QStringLiteral("input"), input)) {
            return DsspApi::Result::fail(DsspApi::validationError(
                QStringLiteral("Missing request field \"input\"")));
        }
        QJsonArray notes;
        if (!DsspApi::readArray(input, QStringLiteral("notes"), notes)) {
            return DsspApi::Result::fail(DsspApi::validationError(
                QStringLiteral("Missing request field \"input.notes\"")));
        }

        QJsonArray outNotes;
        QList<QJsonObject> pendingNotes;
        QList<QJsonObject> results;
        // Group non-skipped notes by language to batch G2P per language.
        QMap<QString, QList<int>> langGroups;
        for (const auto &noteValue : notes) {
            if (!noteValue.isObject()) {
                return DsspApi::Result::fail(DsspApi::validationError(
                    QStringLiteral("Invalid note in \"input.notes\"")));
            }
            const auto note = noteValue.toObject();
            QString lyric;
            QString language;
            if (!DsspApi::readString(note, QStringLiteral("lyric"), lyric) ||
                !DsspApi::readString(note, QStringLiteral("language"), language)) {
                return DsspApi::Result::fail(DsspApi::validationError(
                    QStringLiteral("Invalid note in \"input.notes\"")));
            }
            const auto index = pendingNotes.size();
            pendingNotes.append(note);
            if (isSkippedLyric(lyric.trimmed())) {
                results.append(QJsonObject{
                    {QStringLiteral("pronunciation"), lyric.trimmed()},
                    {QStringLiteral("candidates"), QJsonArray{lyric.trimmed()}},
                });
                continue;
            }
            results.append(QJsonObject{});
            langGroups[language].append(index);
        }

        auto &session = SynthrtEngine::instance().session();
        const auto singerKey = toSingerRef(singerRef);
        const auto packageId = singerRef.packageId.toStdString();
        const auto version = VersionUtils::qt_to_stdc(
            QVersionNumber::fromString(singerRef.version));

        QMap<int, QJsonObject> convertedResults;
        for (auto it = langGroups.constBegin(); it != langGroups.constEnd(); ++it) {
            const auto &language = it.key();
            std::vector<srt::g2p::G2pInput> inputs;
            inputs.reserve(static_cast<size_t>(it.value().size()));
            for (const auto index : it.value()) {
                const auto note = pendingNotes.at(index);
                auto lyric = note.value(QStringLiteral("lyric")).toString();
                while (lyric.endsWith('+'))
                    lyric.chop(1);
                srt::g2p::G2pInput input;
                input.lyric = toUtf8(lyric);
                inputs.push_back(std::move(input));
            }

            const auto langStd = toUtf8(language);
            // Serialize language conversions across HTTP workers: the shared
            // LanguageService ONNX sessions are not reentrant.
            std::lock_guard languageLock(g_languageConversionMutex);
            auto readyExp = session.ensureLanguageReady(packageId, version, langStd);
            if (!readyExp) {
                qCWarning(logDsspLanguage).noquote()
                    << "G2P language ready failed for" << language << ":"
                    << fromUtf8(readyExp.error().message());
                for (const auto index : it.value()) {
                    auto lyric = pendingNotes.at(index).value(QStringLiteral("lyric")).toString();
                    while (lyric.endsWith('+'))
                        lyric.chop(1);
                    convertedResults[index] = QJsonObject{
                        {QStringLiteral("pronunciation"), lyric},
                        {QStringLiteral("candidates"), QJsonArray{lyric}},
                    };
                }
                continue;
            }

            auto exp = session.convertG2p(singerKey, langStd, inputs);
            if (!exp) {
                qCWarning(logDsspLanguage).noquote()
                    << "G2P conversion failed for" << language << ":"
                    << fromUtf8(exp.error().message());
                for (const auto index : it.value()) {
                    auto lyric = pendingNotes.at(index).value(QStringLiteral("lyric")).toString();
                    while (lyric.endsWith('+'))
                        lyric.chop(1);
                    convertedResults[index] = QJsonObject{
                        {QStringLiteral("pronunciation"), lyric},
                        {QStringLiteral("candidates"), QJsonArray{lyric}},
                    };
                }
                continue;
            }

            const auto &outcomes = *exp;
            if (outcomes.size() != inputs.size()) {
                qCWarning(logDsspLanguage).noquote()
                    << "G2P returned" << outcomes.size() << "outcomes for" << inputs.size()
                    << "requests";
                for (const auto index : it.value()) {
                    auto lyric = pendingNotes.at(index).value(QStringLiteral("lyric")).toString();
                    while (lyric.endsWith('+'))
                        lyric.chop(1);
                    convertedResults[index] = QJsonObject{
                        {QStringLiteral("pronunciation"), lyric},
                        {QStringLiteral("candidates"), QJsonArray{lyric}},
                    };
                }
                continue;
            }

            for (size_t i = 0; i < outcomes.size(); ++i) {
                const auto index = it.value()[static_cast<qsizetype>(i)];
                const auto &outcome = outcomes[i];
                QJsonArray candidates;
                for (const auto &candidate : outcome.candidates)
                    candidates.append(fromUtf8(candidate));
                if (candidates.isEmpty())
                    candidates.append(fromUtf8(outcome.pronunciation));
                convertedResults[index] = QJsonObject{
                    {QStringLiteral("pronunciation"), fromUtf8(outcome.pronunciation)},
                    {QStringLiteral("candidates"), candidates},
                };
            }
        }

        for (int i = 0; i < results.size(); ++i) {
            if (const auto it = convertedResults.constFind(i); it != convertedResults.constEnd())
                outNotes.append(it.value());
            else
                outNotes.append(results.at(i));
        }

        return DsspApi::Result::ok(QJsonObject{
            {QStringLiteral("state"), QStringLiteral("COMPLETE")},
            {QStringLiteral("output"),
             QJsonObject{{QStringLiteral("notes"), outNotes}}},
        });
    }

    DsspApi::Result phoneme(const QJsonObject &body) {
        if (!SynthrtEngine::instance().sessionReady()) {
            return DsspApi::Result::fail(
                DsspApi::internalError(QStringLiteral("Inference engine is not ready")));
        }

        DsspMetadata::SingerReference singerRef;
        if (auto problem = resolveSingleSingerContext(body, singerRef))
            return DsspApi::Result::fail(*problem);

        QJsonObject input;
        if (!DsspApi::readObject(body, QStringLiteral("input"), input)) {
            return DsspApi::Result::fail(DsspApi::validationError(
                QStringLiteral("Missing request field \"input\"")));
        }
        QJsonArray notes;
        if (!DsspApi::readArray(input, QStringLiteral("notes"), notes)) {
            return DsspApi::Result::fail(DsspApi::validationError(
                QStringLiteral("Missing request field \"input.notes\"")));
        }

        auto &session = SynthrtEngine::instance().session();
        const auto singerKey = toSingerRef(singerRef);
        const auto packageId = singerRef.packageId.toStdString();
        const auto version = VersionUtils::qt_to_stdc(
            QVersionNumber::fromString(singerRef.version));

        QSet<QString> failedLanguages;
        QSet<QString> readyLanguages;

        QJsonArray outNotes;
        for (const auto &noteValue : notes) {
            if (!noteValue.isObject()) {
                return DsspApi::Result::fail(DsspApi::validationError(
                    QStringLiteral("Invalid note in \"input.notes\"")));
            }
            const auto note = noteValue.toObject();
            QString pronunciation;
            QString language;
            if (!DsspApi::readString(note, QStringLiteral("pronunciation"), pronunciation) ||
                !DsspApi::readString(note, QStringLiteral("language"), language)) {
                return DsspApi::Result::fail(DsspApi::validationError(
                    QStringLiteral("Invalid note in \"input.notes\"")));
            }

            QJsonArray phonemes;
            if (pronunciation == QStringLiteral("SP") || pronunciation == QStringLiteral("AP")) {
                phonemes.append(QJsonObject{
                    {QStringLiteral("token"), pronunciation},
                    {QStringLiteral("onset"), true},
                });
            } else if (pronunciation == QStringLiteral("-") || pronunciation.isEmpty()) {
                // Slur note without phonemes: leave empty.
            } else if (!failedLanguages.contains(language)) {
                // Serialize language conversions across HTTP workers: the shared
                // LanguageService ONNX sessions are not reentrant.
                std::lock_guard languageLock(g_languageConversionMutex);
                if (!readyLanguages.contains(language)) {
                    const auto langStd = toUtf8(language);
                    auto readyExp = session.ensureLanguageReady(packageId, version, langStd);
                    if (!readyExp) {
                        qCWarning(logDsspLanguage).noquote()
                            << "S2P language ready failed for" << language << ":"
                            << fromUtf8(readyExp.error().message());
                        failedLanguages.insert(language);
                        return DsspApi::Result::fail(
                            DsspApi::internalError(QStringLiteral("S2P conversion failed: %1")
                                                       .arg(fromUtf8(readyExp.error().message()))));
                    }
                    readyLanguages.insert(language);
                }

                auto exp = session.convertS2p(singerKey, toUtf8(language),
                                              toUtf8(pronunciation));
                if (!exp) {
                    qCWarning(logDsspLanguage).noquote()
                        << "S2P conversion failed for" << pronunciation << ":"
                        << fromUtf8(exp.error().message());
                    failedLanguages.insert(language);
                    return DsspApi::Result::fail(
                        DsspApi::internalError(QStringLiteral("S2P conversion failed: %1")
                                                   .arg(fromUtf8(exp.error().message()))));
                }

                const auto &syllable = *exp;
                for (size_t k = 0; k < syllable.phonemes.size(); ++k) {
                    phonemes.append(QJsonObject{
                        {QStringLiteral("token"), fromUtf8(syllable.phonemes[k])},
                        {QStringLiteral("onset"),
                         (k < syllable.onsets.size()) ? syllable.onsets[k] : false},
                    });
                }
            }

            outNotes.append(QJsonObject{{QStringLiteral("phonemes"), phonemes}});
        }

        return DsspApi::Result::ok(QJsonObject{
            {QStringLiteral("state"), QStringLiteral("COMPLETE")},
            {QStringLiteral("output"),
             QJsonObject{{QStringLiteral("notes"), outNotes}}},
        });
    }

} // namespace DsspLanguage
