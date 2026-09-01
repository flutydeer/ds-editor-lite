#include "PackageManager.h"

#include <lite/Support/StringUtils.h>
#include <lite/Support/VersionUtils.h>
#include <lite/PackageManager/Models/PackageInfo.h>
#include <lite/ProjectModel/Voice/SingerInfo.h>
#include <lite/Tasking/TaskManager.h>
#include <lite/PackageManager/Tasks/GetInstalledPackagesTask.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>

#include <stdcorelib/path.h>
#include <stdcorelib/system.h>

#include <synthrt/Core/Support/DisplayText.h>
#include <synthrt/Core/Support/JSON.h>
#include <diffsinger/Bank/PackageManifest.h>
#include <diffsinger/Bank/SingerManifest.h>

#include <QDebug>
#include <QElapsedTimer>
#include <QLocale>
#include <QMutexLocker>
#include <QSet>

#if defined(Q_OS_MAC)
#  include <lite/Support/MacOSUtils.h>
#endif

namespace fs = std::filesystem;

namespace {
    const ds::bank::InferenceInfo *findInference(const ds::bank::PackageManifest &manifest,
                                                 const std::string &id,
                                                 const std::string &className) {
        for (const auto &inference : manifest.inferences()) {
            if (inference.id == id && inference.className == className)
                return &inference;
        }
        return nullptr;
    }

    /// Copies all translations of a synthrt DisplayText into a Qt map
    /// (language tag -> text, keys kept verbatim per ds-spec 2.4) so the host
    /// can re-resolve the display text per UI language without rescanning the
    /// voicebank.
    QMap<QString, QString> toLocalizedTextMap(const srt::core::DisplayText &text) {
        QMap<QString, QString> map;
        for (const auto &locale : text.locales()) {
            if (const auto *value = text.text(locale))
                map.insert(QString::fromStdString(locale), QString::fromStdString(*value));
        }
        return map;
    }

    QStringList toQStringList(const std::vector<std::string> &values) {
        QStringList result;
        result.reserve(static_cast<QStringList::size_type>(values.size()));
        for (const auto &value : values)
            result.append(QString::fromStdString(value));
        return result;
    }

    std::vector<const ds::bank::InferenceInfo *>
        stageCandidates(const ds::bank::PackageManifest &owningManifest,
                        const std::vector<ds::bank::PackageManifest> &manifests,
                        const std::string &id, const std::string &className) {
        if (const auto *inference = findInference(owningManifest, id, className))
            return {inference};

        std::vector<const ds::bank::InferenceInfo *> candidates;
        for (const auto &manifest : manifests) {
            if (const auto *inference = findInference(manifest, id, className))
                candidates.push_back(inference);
        }
        return candidates;
    }

    std::vector<const ds::bank::InferenceInfo *>
        reportedStageCandidates(const ds::bank::PackageManifest &owningManifest,
                                const std::vector<ds::bank::PackageManifest> &manifests,
                                const ds::bank::SingerCapabilityReport &report,
                                const std::string &className) {
        for (const auto &stage : report.stages) {
            if (stage.className == className)
                return stageCandidates(owningManifest, manifests, stage.stageId, className);
        }
        return {};
    }

    std::vector<const ds::bank::InferenceInfo *>
        importedStageCandidates(const ds::bank::PackageManifest &owningManifest,
                                const std::vector<ds::bank::PackageManifest> &manifests,
                                const ds::bank::SingerManifest &singer,
                                const std::string &className) {
        std::vector<const ds::bank::InferenceInfo *> candidates;
        for (const auto &stageImport : singer.imports()) {
            if (const auto *inference =
                    findInference(owningManifest, stageImport.inferenceId, className)) {
                candidates.push_back(inference);
            }
        }
        if (!candidates.empty())
            return candidates;

        for (const auto &stageImport : singer.imports()) {
            for (const auto &manifest : manifests) {
                if (const auto *inference =
                        findInference(manifest, stageImport.inferenceId, className)) {
                    candidates.push_back(inference);
                }
            }
        }
        return candidates;
    }

    std::optional<QStringList>
        acousticParameters(const ds::bank::PackageManifest &owningManifest,
                           const std::vector<ds::bank::PackageManifest> &manifests,
                           const ds::bank::SingerCapabilityReport &report) {
        const auto candidates =
            reportedStageCandidates(owningManifest, manifests, report, "ai.svs.AcousticInference");
        std::optional<QStringList> result;
        for (const auto *candidate : candidates) {
            const auto parameters = toQStringList(candidate->parameters);
            if (result && *result != parameters)
                return std::nullopt;
            result = parameters;
        }
        return result;
    }

    std::optional<bool> configurationFlag(const ds::bank::InferenceInfo &inference,
                                          const std::string &name, const bool defaultValue) {
        std::ifstream file(inference.configPath);
        if (!file.is_open())
            return std::nullopt;

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string parseError;
        const auto root = srt::core::JsonValue::fromJson(buffer.str(), true, &parseError);
        if (!parseError.empty() || !root.isObject())
            return std::nullopt;

        const auto &rootObject = root.toObject();
        const auto configurationIt = rootObject.find("configuration");
        if (configurationIt == rootObject.end() || !configurationIt->second.isObject())
            return std::nullopt;

        const auto &configuration = configurationIt->second.toObject();
        const auto valueIt = configuration.find(name);
        if (valueIt == configuration.end())
            return defaultValue;
        if (!valueIt->second.isBool())
            return std::nullopt;
        return valueIt->second.toBool();
    }

    std::optional<bool>
        consistentConfigurationFlag(const std::vector<const ds::bank::InferenceInfo *> &candidates,
                                    const std::string &name, const bool defaultValue) {
        std::optional<bool> result;
        for (const auto *candidate : candidates) {
            const auto value = configurationFlag(*candidate, name, defaultValue);
            if (!value || (result && *result != *value))
                return std::nullopt;
            result = value;
        }
        return result;
    }

    std::optional<bool> reportedStageFlag(const ds::bank::PackageManifest &owningManifest,
                                          const std::vector<ds::bank::PackageManifest> &manifests,
                                          const ds::bank::SingerCapabilityReport &report,
                                          const std::string &className, const std::string &name,
                                          const bool defaultValue) {
        return consistentConfigurationFlag(
            reportedStageCandidates(owningManifest, manifests, report, className), name,
            defaultValue);
    }

    std::optional<bool> importedStageFlag(const ds::bank::PackageManifest &owningManifest,
                                          const std::vector<ds::bank::PackageManifest> &manifests,
                                          const ds::bank::SingerManifest &singer,
                                          const std::string &className, const std::string &name,
                                          const bool defaultValue) {
        return consistentConfigurationFlag(
            importedStageCandidates(owningManifest, manifests, singer, className), name,
            defaultValue);
    }
}

PackageManager::PackageManager(QObject *parent) : QObject(parent) {
}

PackageManager::~PackageManager() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(PackageManager)

void PackageManager::initialize(const QStringList &searchPaths) {
    std::call_once(m_initialized, [this, searchPaths]() {
        Q_EMIT moduleStatusChanged(ModuleStatus::Loading);
        auto task = new GetInstalledPackagesTask(searchPaths);
        connect(task, &GetInstalledPackagesTask::finished, this, [this, task]() {
            taskManager->removeTask(task);
            if (task->result) {
                Q_EMIT moduleStatusChanged(ModuleStatus::Ready);
            } else {
                qCritical() << "Package scan failed:" << task->result.getError().message;
                Q_EMIT moduleStatusChanged(ModuleStatus::Error);
            }
            delete task;
        });
        taskManager->addAndStartTask(task);
    });
}

Expected<GetInstalledPackagesResult, GetInstalledPackagesError>
    PackageManager::refreshInstalledPackages(const QStringList &searchPathsQt,
                                             RefreshCommitGate commitGate) {
    {
        std::unique_lock lock(m_refreshMutex);
        if (m_refreshing) {
            qDebug() << "Already refreshing, wait for completion";
            m_refreshCompleted.wait(lock, [this] { return !m_refreshing; });
            if (!m_lastRefreshCommitRejected) {
                auto result = m_lastRefreshResult;
                lock.unlock();
                if (result && commitGate)
                    commitGate();
                return result;
            }
            qDebug() << "Leading package refresh was not committed, retry scan";
        }
        m_refreshing = true;
    }

    bool commitRejected = false;
    auto completed =
        [this, &searchPathsQt, &commitGate,
         &commitRejected]() -> Expected<GetInstalledPackagesResult, GetInstalledPackagesError> {
        QElapsedTimer timer;
        timer.start();
        GetInstalledPackagesResult result;

        std::vector<fs::path> searchPaths;
        for (const auto &pathQt : searchPathsQt) {
            const auto path = StringUtils::qstr_to_path(pathQt);
            std::error_code error;
            const bool exists = fs::exists(path, error);
            const bool isDirectory = exists && fs::is_directory(path, error);
            if (error || !isDirectory) {
                const auto message = error ? tr("Unable to access directory: %1")
                                                 .arg(QString::fromStdString(error.message()))
                                           : tr("Path is not a valid directory");
                result.failedPackages.emplace_back(pathQt, message);
                continue;
            }
            searchPaths.push_back(path);
        }

        const bool allowReuse = m_catalogGeneration == 0;
        // SynthrtEngine::initialize is triggered asynchronously by InferEngine
        // on a separate task. VoicebankSession (Stage 1: voicebank scan +
        // LanguageService metadata) must be ready before we can query the
        // snapshot. We wait on sessionReady() rather than initialized() so
        // PackageManager doesn't block on Stage 2 (ONNX model loading), which
        // is slow and not needed for package enumeration. Uses a condition
        // variable internally — no polling.
        //
        // If initialize() finishes (success or failure) without the session
        // becoming ready, waitForSession returns true but refreshVoicebanks
        // below will surface the actual error (e.g. "session not initialized"
        // when Stage 1's refresh failed).
        if (!SynthrtEngine::instance().sessionReady()) {
            if (!SynthrtEngine::instance().waitForSession()) {
                return GetInstalledPackagesError{
                    GetInstalledPackagesErrorType::MetadataBackendNotInitialized,
                    QStringLiteral("SynthrtEngine session initialization timed out"),
                };
            }
        }
        auto snapshotExp = SynthrtEngine::instance().refreshVoicebanks(searchPaths, allowReuse);
        if (!snapshotExp) {
            return GetInstalledPackagesError{
                GetInstalledPackagesErrorType::MetadataBackendNotInitialized,
                QString::fromUtf8(snapshotExp.error().message()),
            };
        }
        const auto snapshot = *snapshotExp;

        // Iterate packages (valid + invalid). For valid packages, look up the
        // manifest via VoicebankSnapshot::findManifest(). Singers are looked up
        // by matching singer.ref.packageId + singer.ref.version to the package.
        for (const auto &status : snapshot->packages) {
            if (!status.valid) {
                result.failedPackages.emplace_back(StringUtils::path_to_qstr(status.rootPath),
                                                   QString::fromStdString(status.error.message));
                continue;
            }

            const auto packageId = QString::fromStdString(status.packageId);
            const auto packageVersion = VersionUtils::stdc_to_qt(status.version);

            const auto *manifest = snapshot->findManifest(status.packageId, status.version);
            if (!manifest) {
                result.failedPackages.emplace_back(
                    StringUtils::path_to_qstr(status.rootPath),
                    QStringLiteral("Manifest not available for package %1").arg(packageId));
                continue;
            }

            const auto vendorText = manifest->author();
            const auto descriptionText = manifest->description();
            const auto licenseText = manifest->license();
            PackageInfo packageInfo(packageId, packageVersion,
                                    QString::fromStdString(vendorText.text()),
                                    QString::fromStdString(descriptionText.text()),
                                    QString::fromStdString(licenseText.text()), {}, {},
                                    StringUtils::path_to_qstr(status.rootPath));
            packageInfo.setLocalizedVendor(toLocalizedTextMap(vendorText));
            packageInfo.setLocalizedDescription(toLocalizedTextMap(descriptionText));
            packageInfo.setLocalizedLicense(toLocalizedTextMap(licenseText));

            // Find singers belonging to this package version.
            for (const auto &singerSnapshot : snapshot->singers) {
                if (singerSnapshot.ref.packageId != status.packageId ||
                    singerSnapshot.ref.version != status.version.toString()) {
                    continue;
                }

                QList<LanguageInfo> languageInfos;
                QList<SpeakerInfo> speakerInfos;
                const ds::bank::SingerManifest *singerManifest = nullptr;
                for (const auto &singer : manifest->singers()) {
                    if (singer.singerId() != singerSnapshot.ref.singerId) {
                        continue;
                    }
                    singerManifest = &singer;
                    for (const auto &lang : singer.languages()) {
                        LanguageInfo langInfo(QString::fromStdString(lang.languageId()),
                                              QString::fromStdString(lang.name().text()),
                                              QString::fromStdString(lang.g2pId()),
                                              StringUtils::path_to_qstr(lang.dict()),
                                              QString::fromStdString(lang.s2pMode()),
                                              QString::fromStdString(lang.onsetMode()),
                                              StringUtils::path_to_qstr(lang.s2pFile()),
                                              StringUtils::path_to_qstr(lang.onsetFile()));
                        if (lang.hasG2pPackageVersion()) {
                            langInfo.setG2pPackageVersion(
                                QString::fromStdString(lang.g2pPackageVersion().toString()));
                        }
                        QStringList g2pPaths;
                        g2pPaths.reserve(
                            static_cast<QStringList::size_type>(lang.g2pPackages().size()));
                        for (const auto &p : lang.g2pPackages()) {
                            g2pPaths << StringUtils::path_to_qstr(p);
                        }
                        langInfo.setG2pPackagePaths(g2pPaths);
                        langInfo.setLocalizedNames(toLocalizedTextMap(lang.name()));
                        languageInfos.append(std::move(langInfo));
                    }
                    for (const auto &spk : singer.speakers()) {
                        SpeakerInfo liteSpk(QString::fromStdString(spk.speakerId()),
                                            QString::fromStdString(spk.name().text()));
                        // B-13 lite 侧: toneRange 映射 + 兼容旧 toneMin/toneMax QString
                        liteSpk.setLocalizedNames(toLocalizedTextMap(spk.name()));
                        if (spk.toneRange()) {
                            const auto lo = spk.toneRange()->first;
                            const auto hi = spk.toneRange()->second;
                            liteSpk.setToneRange(std::make_pair(lo, hi));
                            // Legacy tone range fields are serialized protocol values.
                            liteSpk.setToneMin(QString::number(lo));
                            liteSpk.setToneMax(QString::number(hi));
                        }
                        speakerInfos.emplace_back(std::move(liteSpk));
                    }
                    break;
                }

                if (languageInfos.isEmpty()) {
                    for (const auto &langInfo : singerSnapshot.languageInfos) {
                        const auto id = QString::fromStdString(langInfo.languageId());
                        languageInfos.emplace_back(id, id);
                    }
                }
                if (speakerInfos.isEmpty()) {
                    for (const auto &spkInfo : singerSnapshot.speakerInfos) {
                        const auto id = QString::fromStdString(spkInfo.speakerId());
                        speakerInfos.emplace_back(id, id);
                    }
                }

                // 从 snapshot.capabilityReport 提取 lite 侧 capability 摘要
                // 并标记每个 liteSpk.mixable（mixableSpeakers 集合成员）。
                // 纯 G2P 包或 Inconsistent 声库 capabilityReport 为 nullopt / mixableSpeakers 空，
                // lite UI 据此展示降级信息。
                std::optional<SingerCapabilitySummary> capSummary;
                if (singerSnapshot.capabilityReport) {
                    const auto &report = *singerSnapshot.capabilityReport;
                    SingerCapabilitySummary summary;
                    for (const auto &spk : report.mixableSpeakers)
                        summary.mixableSpeakers.append(QString::fromStdString(spk));
                    summary.speakerConsistency = static_cast<int>(report.speakerConsistency);
                    for (const auto &w : report.speakerWarnings)
                        summary.speakerWarnings.append(QString::fromStdString(w));
                    summary.acousticParameters =
                        acousticParameters(*manifest, snapshot->manifests, report);
                    summary.pitchUsesExpressiveness =
                        reportedStageFlag(*manifest, snapshot->manifests, report,
                                          "ai.svs.PitchInference", "useExpressiveness", true);
                    if (singerManifest) {
                        summary.vocoderPitchControllable = importedStageFlag(
                            *manifest, snapshot->manifests, *singerManifest,
                            "ai.svs.VocoderInference", "pitchControllable", false);
                    }

                    for (const auto &ph : report.effectivePhonemes)
                        summary.effectivePhonemes.append(QString::fromStdString(ph));
                    summary.phonemeConsistency = static_cast<int>(report.phonemeConsistency);
                    for (const auto &w : report.phonemeWarnings)
                        summary.phonemeWarnings.append(QString::fromStdString(w));
                    summary.phonemeDegraded = report.phonemeDegraded;

                    for (const auto &lang : report.effectiveLanguages)
                        summary.effectiveLanguages.append(QString::fromStdString(lang));
                    summary.languageConsistency = static_cast<int>(report.languageConsistency);
                    for (const auto &w : report.languageWarnings)
                        summary.languageWarnings.append(QString::fromStdString(w));
                    capSummary = std::move(summary);

                    // 标记每个 liteSpk.mixable（singer 域名匹配）
                    QSet<QString> mixableSet;
                    for (const auto &spk : report.mixableSpeakers)
                        mixableSet.insert(QString::fromStdString(spk));
                    for (auto &liteSpk : speakerInfos)
                        liteSpk.setMixable(mixableSet.contains(liteSpk.id()));
                }

                SingerInfo singerInfo(
                    SingerIdentifier{QString::fromStdString(singerSnapshot.ref.singerId), packageId,
                                     packageVersion},
                    QString::fromStdString(singerSnapshot.name.text()), std::move(speakerInfos),
                    std::move(languageInfos),
                    QString::fromStdString(singerSnapshot.defaultLanguage));
                singerInfo.setLocalizedNames(toLocalizedTextMap(singerSnapshot.name));
                singerInfo.setCapability(std::move(capSummary));
                switch (singerSnapshot.resolutionState) {
                    case ds::bank::ResolutionState::Resolved:
                        singerInfo.setResolutionState(ResolutionState::Resolved);
                        break;
                    case ds::bank::ResolutionState::Missing:
                        singerInfo.setResolutionState(ResolutionState::Missing);
                        break;
                    case ds::bank::ResolutionState::Pending:
                    default:
                        singerInfo.setResolutionState(ResolutionState::Pending);
                        break;
                }
                packageInfo.addSinger(singerInfo);
            }
            result.successfulPackages.append(std::move(packageInfo));
        }

        qDebug() << "Package scan completed in" << timer.elapsed() << "ms";
        if (commitGate && !commitGate()) {
            commitRejected = true;
            return result;
        }
        {
            QWriteLocker writeLocker(&m_resultRwLock);
            m_result = result;
            m_catalogGeneration = snapshot->generation;
            m_packageLocator.clear();
            m_singerLocator.clear();
            for (const auto &packageInfo : std::as_const(m_result.successfulPackages)) {
                for (const auto &singerInfo : packageInfo.singers()) {
                    m_packageLocator.insert(singerInfo.identifier(), packageInfo);
                    m_singerLocator.insert(singerInfo.identifier(), singerInfo);
                }
            }
        }
        return result;
    }();

    std::optional<GetInstalledPackagesResult> retainedResult;
    if (commitRejected)
        retainedResult = installedPackages();
    QList<PackageInfo> refreshedPackages;
    {
        std::lock_guard lock(m_refreshMutex);
        m_lastRefreshResult = retainedResult
                                  ? Expected<GetInstalledPackagesResult, GetInstalledPackagesError>(
                                        std::move(*retainedResult))
                                  : completed;
        m_lastRefreshCommitRejected = commitRejected;
        m_refreshing = false;
        if (completed && !commitRejected) {
            refreshedPackages = completed.get().successfulPackages;
        }
    }
    m_refreshCompleted.notify_all();
    if (completed && !commitRejected) {
        Q_EMIT packagesRefreshed(refreshedPackages);
    }
    return completed;
}

GetInstalledPackagesResult PackageManager::installedPackages() const {
    QReadLocker readLocker(&m_resultRwLock);
    return m_result;
}

PackageInfo PackageManager::findPackageByIdentifier(const SingerIdentifier &identifier) const {
    QReadLocker readLocker(&m_resultRwLock);
    const auto it = m_packageLocator.constFind(identifier);
    if (it == m_packageLocator.constEnd()) {
        return {};
    }
    return it.value();
}

SingerInfo PackageManager::findSingerByIdentifier(const SingerIdentifier &identifier) const {
    QReadLocker readLocker(&m_resultRwLock);
    const auto it = m_singerLocator.constFind(identifier);
    if (it == m_singerLocator.constEnd()) {
        return {};
    }
    return it.value();
}

QString PackageManager::srtErrorToString(const srt::core::Error &error) {
    // v4: use ErrorCode system (error.codeString() returns e.g.
    // "Package::ManifestInvalid", "Inference::ModelLoadFailed") instead of
    // the deprecated Error::Type enum which only had 10 generic values and
    // lost all Package/Inference/G2P/Driver/S2P/SVS categorization.
    const QString code = QString::fromLatin1(error.codeString());
    const QString message = QString::fromStdString(error.message());
    if (error.ok()) {
        return tr("No error: ") + message;
    }
    return QStringLiteral("[%1] %2").arg(code, message);
}
