//
// Created by FlutyDeer on 2025/7/27.
//

#include "PackageManager.h"

#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Utils/StringUtils.h"
#include "Utils/VersionUtils.h"
#include "Models/PackageInfo.h"
#include "Models/SingerInfo.h"
#include "Modules/Task/TaskManager.h"
#include "Tasks/GetInstalledPackagesTask.h"
#include "Modules/SynthrtEngine/SynthrtEngine.h"

#include <filesystem>

#include <stdcorelib/path.h>
#include <stdcorelib/system.h>

#include <diffsinger/Bank/PackageManifest.h>
#include <diffsinger/Bank/SingerManifest.h>

#include <QDebug>
#include <QElapsedTimer>
#include <QLocale>
#include <QMutexLocker>
#include <QSet>

#if defined(Q_OS_MAC)
#  include "Utils/MacOSUtils.h"
#endif

namespace fs = std::filesystem;

PackageManager::PackageManager(QObject *parent) : QObject(parent) {
}

PackageManager::~PackageManager() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(PackageManager)

void PackageManager::initialize() {
    std::call_once(m_initialized, [this]() {
        appStatus->packageModuleStatus = AppStatus::ModuleStatus::Loading;
        auto task = new GetInstalledPackagesTask;
        connect(task, &GetInstalledPackagesTask::finished, this, [task]() {
            taskManager->removeTask(task);
            if (task->result) {
                appStatus->packageModuleStatus = AppStatus::ModuleStatus::Ready;
            } else {
                qCritical() << "Package scan failed:" << task->result.getError().message;
                appStatus->packageModuleStatus = AppStatus::ModuleStatus::Error;
            }
            delete task;
        });
        taskManager->addAndStartTask(task);
    });
}

Expected<GetInstalledPackagesResult, GetInstalledPackagesError>
    PackageManager::refreshInstalledPackages() {
    {
        std::unique_lock lock(m_refreshMutex);
        if (m_refreshing) {
            qDebug() << "Already refreshing, wait for completion";
            m_refreshCompleted.wait(lock, [this] { return !m_refreshing; });
            return m_lastRefreshResult;
        }
        m_refreshing = true;
    }

    auto completed = [this]() -> Expected<GetInstalledPackagesResult, GetInstalledPackagesError> {
        QElapsedTimer timer;
        timer.start();
        GetInstalledPackagesResult result;

        std::vector<fs::path> searchPaths;
        for (const auto &pathQt : std::as_const(appOptions->general()->packageSearchPaths)) {
            const auto path = StringUtils::qstr_to_path(pathQt);
            if (!fs::exists(path) || !fs::is_directory(path)) {
                result.failedPackages.emplace_back(pathQt, tr("Path is not a valid directory"));
                continue;
            }
            searchPaths.push_back(path);
        }

        const bool allowReuse = m_catalogGeneration == 0;
        auto catalogExp = SynthrtEngine::instance().refreshVoicebanks(searchPaths, allowReuse);
        if (!catalogExp) {
            return GetInstalledPackagesError{
                GetInstalledPackagesErrorType::MetadataBackendNotInitialized,
                QString::fromUtf8(catalogExp.error().message()),
            };
        }
        const auto catalog = *catalogExp;

        for (const auto &record : catalog->packages) {
            const auto &status = record.status;
            if (!status.valid) {
                result.failedPackages.emplace_back(StringUtils::path_to_qstr(status.rootPath),
                                                   QString::fromStdString(status.error.message));
                continue;
            }
            if (record.parseError) {
                result.failedPackages.emplace_back(
                    StringUtils::path_to_qstr(status.rootPath),
                    QString::fromStdString(record.parseError->message()));
                continue;
            }

            const auto &manifest = *record.manifest;
            const auto packageId = QString::fromStdString(status.packageId);
            const auto packageVersion = VersionUtils::stdc_to_qt(status.version);
            PackageInfo packageInfo(packageId, packageVersion,
                                    QString::fromStdString(manifest.author()),
                                    QString::fromStdString(manifest.description()),
                                    QString::fromStdString(manifest.license()), {}, {},
                                    StringUtils::path_to_qstr(status.rootPath));

            for (const auto &snapshot : record.singers) {
                QList<LanguageInfo> languageInfos;
                QList<SpeakerInfo> speakerInfos;
                for (const auto &singer : manifest.singers()) {
                    if (singer.singerId() != snapshot.ref.singerId) {
                        continue;
                    }
                    for (const auto &lang : singer.languages()) {
                        LanguageInfo langInfo(QString::fromStdString(lang.languageId()),
                                              QString::fromStdString(lang.name()),
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
                        languageInfos.append(std::move(langInfo));
                    }
                    for (const auto &spk : singer.speakers()) {
                        SpeakerInfo liteSpk(QString::fromStdString(spk.speakerId()),
                                            QString::fromStdString(spk.name()));
                        // B-13 lite 侧: toneRange 映射 + 兼容旧 toneMin/toneMax QString
                        if (spk.toneRange()) {
                            const auto lo = spk.toneRange()->first;
                            const auto hi = spk.toneRange()->second;
                            liteSpk.setToneRange(std::make_pair(lo, hi));
                            liteSpk.setToneMin(QString::number(lo));
                            liteSpk.setToneMax(QString::number(hi));
                        }
                        speakerInfos.emplace_back(std::move(liteSpk));
                    }
                    break;
                }

                if (languageInfos.isEmpty()) {
                    for (const auto &languageId : snapshot.languages) {
                        const auto id = QString::fromStdString(languageId);
                        languageInfos.emplace_back(id, id);
                    }
                }
                if (speakerInfos.isEmpty()) {
                    for (const auto &speakerId : snapshot.speakerIds) {
                        const auto id = QString::fromStdString(speakerId);
                        speakerInfos.emplace_back(id, id);
                    }
                }

                // 从 snapshot.capabilityReport 提取 lite 侧 capability 摘要
                // 并标记每个 liteSpk.mixable（mixableSpeakers 集合成员）。
                // 纯 G2P 包或 Inconsistent 声库 capabilityReport 为 nullopt / mixableSpeakers 空，
                // lite UI 据此展示降级信息。
                std::optional<SingerCapabilitySummary> capSummary;
                if (snapshot.capabilityReport) {
                    const auto &report = *snapshot.capabilityReport;
                    SingerCapabilitySummary summary;
                    for (const auto &spk : report.mixableSpeakers)
                        summary.mixableSpeakers.append(QString::fromStdString(spk));
                    summary.speakerConsistency = static_cast<int>(report.speakerConsistency);
                    for (const auto &w : report.speakerWarnings)
                        summary.speakerWarnings.append(QString::fromStdString(w));

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
                    SingerIdentifier{QString::fromStdString(snapshot.ref.singerId), packageId,
                                     packageVersion},
                    QString::fromStdString(snapshot.name), std::move(speakerInfos),
                    std::move(languageInfos), QString::fromStdString(snapshot.defaultLanguage));
                singerInfo.setCapability(std::move(capSummary));
                switch (snapshot.resolutionState) {
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
        {
            QWriteLocker writeLocker(&m_resultRwLock);
            m_result = result;
            m_catalogGeneration = catalog->generation;
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

    QList<PackageInfo> refreshedPackages;
    {
        std::lock_guard lock(m_refreshMutex);
        m_lastRefreshResult = completed;
        m_refreshing = false;
        if (completed) {
            refreshedPackages = completed.get().successfulPackages;
        }
    }
    m_refreshCompleted.notify_all();
    if (completed) {
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
