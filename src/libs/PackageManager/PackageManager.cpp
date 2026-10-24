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

#include <synthrt/Support/DisplayText.h>

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
    GetInstalledPackagesError commitRejectedError() {
        return {GetInstalledPackagesErrorType::CommitRejected,
                QStringLiteral("The refresh was not committed for this request")};
    }

    /// Preserve all locale keys so the UI can change language without rescanning.
    QMap<QString, QString> toLocalizedTextMap(const srt::DisplayText &text) {
        QMap<QString, QString> map;
        for (const auto &locale : text.locales()) {
            map.insert(QString::fromStdString(locale),
                       QString::fromStdString(text.text(locale)));
        }
        return map;
    }

    QString toQString(const std::string &value) {
        return QString::fromStdString(value);
    }

    template <class Container>
    QStringList toQStringList(const Container &values) {
        QStringList result;
        result.reserve(static_cast<QStringList::size_type>(values.size()));
        for (const auto &value : values) {
            result.append(toQString(value));
        }
        return result;
    }

    /// Map parsed singer capabilities to the editor's capability summary.
    SingerCapabilitySummary summaryOf(const lite::synthrt::SingerCapabilities &capabilities) {
        SingerCapabilitySummary summary;
        for (const auto &speaker : capabilities.speakers) {
            summary.mixableSpeakers.append(toQString(speaker.id));
        }
        QStringList parameters = toQStringList(capabilities.varianceControls);
        parameters.append(toQStringList(capabilities.transitionControls));
        parameters.sort();
        summary.acousticParameters = std::move(parameters);
        summary.pitchUsesExpressiveness = capabilities.allowsExpressiveness;
        summary.effectiveLanguages = toQStringList(capabilities.languages);
        summary.reservedPhonemes = toQStringList(capabilities.reservedPhonemes);
        return summary;
    }

    QList<SpeakerInfo> speakersOf(const lite::synthrt::SingerCapabilities &capabilities) {
        QList<SpeakerInfo> result;
        result.reserve(static_cast<qsizetype>(capabilities.speakers.size()));
        for (const auto &speaker : capabilities.speakers) {
            SpeakerInfo info(toQString(speaker.id), toQString(speaker.name.text()));
            info.setLocalizedNames(toLocalizedTextMap(speaker.name));
            if (speaker.toneRange) {
                info.setToneRange(*speaker.toneRange);
                info.setToneMin(QString::number(speaker.toneRange->first));
                info.setToneMax(QString::number(speaker.toneRange->second));
            }
            // All acoustic model speakers share the same mixing capability.
            info.setMixable(true);
            result.append(std::move(info));
        }
        return result;
    }

    QList<LanguageInfo> languagesOf(const lite::synthrt::SingerCapabilities &capabilities) {
        QList<LanguageInfo> result;
        result.reserve(static_cast<qsizetype>(capabilities.languages.size()));
        for (const auto &handle : capabilities.languages) {
            LanguageInfo language(toQString(handle), toQString(handle));
            if (const auto name = capabilities.languageNames.find(handle);
                name != capabilities.languageNames.end()) {
                language.setName(toQString(name->second.text()));
                language.setLocalizedNames(toLocalizedTextMap(name->second));
            }
            // Preserve the linguist's default lyric as language metadata.
            if (const auto lyric = capabilities.defaultLyrics.find(handle);
                lyric != capabilities.defaultLyrics.end()) {
                language.setDefaultLyric(toQString(lyric->second));
            }
            result.append(std::move(language));
        }
        return result;
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
        connect(
            task, &GetInstalledPackagesTask::finished, this,
            [this, task]() {
                taskManager->removeTask(task);
                if (task->result) {
                    Q_EMIT moduleStatusChanged(ModuleStatus::Ready);
                } else {
                    qCritical() << "Package scan failed:" << task->result.getError().message;
                    Q_EMIT moduleStatusChanged(ModuleStatus::Error);
                }
                delete task;
            },
            Qt::QueuedConnection);
        taskManager->addAndStartTask(task);
    });
}

Expected<GetInstalledPackagesResult, GetInstalledPackagesError>
    PackageManager::refreshInstalledPackages(const QStringList &searchPathsQt,
                                             RefreshCommitGate commitGate, const ScanMode mode) {
    {
        std::unique_lock lock(m_refreshMutex);
        if (m_refreshing) {
            qDebug() << "Already refreshing, wait for completion";
            m_refreshCompleted.wait(lock, [this] { return !m_refreshing; });
            if (!m_lastRefreshCommitRejected && m_refreshMode == mode &&
                m_refreshSearchPaths == searchPathsQt) {
                auto result = m_lastRefreshResult;
                lock.unlock();
                // Sharing a committed refresh does not bypass the follower's commit gate.
                if (result && commitGate && !commitGate())
                    return commitRejectedError();
                return result;
            }
            qDebug()
                << "Leading package refresh was rejected or used different parameters, retry scan";
        }
        m_refreshing = true;
        m_refreshMode = mode;
        m_refreshSearchPaths = searchPathsQt;
    }

    bool commitRejected = false;
    auto runScan =
        [this, &searchPathsQt, &commitGate, &commitRejected,
         mode]() -> Expected<GetInstalledPackagesResult, GetInstalledPackagesError> {
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

        // Wait for asynchronous engine initialization before the first package scan.
        if (!SynthrtEngine::instance().initializationDone()) {
            if (!SynthrtEngine::instance().waitForInitialization()) {
                return GetInstalledPackagesError{
                    GetInstalledPackagesErrorType::MetadataBackendNotInitialized,
                    QStringLiteral("SynthrtEngine initialization timed out"),
                };
            }
        }

        // The SDK scan publishes inference state, so authorization must precede it.
        if (commitGate && !commitGate()) {
            commitRejected = true;
            return commitRejectedError();
        }
        std::vector<lite::synthrt::PackageProblem> problems;
        std::vector<lite::synthrt::PackageEntry> scannedPackages;
        auto scanned = SynthrtEngine::instance().refreshVoicebanks(
            searchPaths, &problems,
            mode == ScanMode::ReuseLoaded ? SynthrtEngine::RescanMode::ReuseLoaded
                                          : SynthrtEngine::RescanMode::Reload,
            &scannedPackages);
        if (!scanned) {
            return GetInstalledPackagesError{
                GetInstalledPackagesErrorType::MetadataBackendNotInitialized,
                QString::fromStdString(scanned.error().message()),
            };
        }
        const auto singers = scanned.take();

        for (const auto &problem : problems) {
            result.failedPackages.emplace_back(StringUtils::path_to_qstr(problem.path),
                                               QString::fromStdString(problem.reason));
        }

        // Keep packages without singers and group by the identity selected by the loader.
        QList<PackageInfo> packages;
        QHash<QString, qsizetype> packageAt;
        for (const auto &entry : scannedPackages) {
            const auto id = toQString(entry.id);
            const auto version = VersionUtils::stdc_to_qt(entry.version);
            PackageInfo info(id, version, toQString(entry.vendor.text()),
                             toQString(entry.description.text()), toQString(entry.copyright.text()),
                             {}, toQString(entry.url), StringUtils::path_to_qstr(entry.path));
            info.setLocalizedVendor(toLocalizedTextMap(entry.vendor));
            info.setLocalizedDescription(toLocalizedTextMap(entry.description));
            info.setLocalizedLicense(toLocalizedTextMap(entry.copyright));
            packageAt.insert(id + QLatin1Char('@') + version.toString(), packages.size());
            packages.append(std::move(info));
        }
        for (const auto &singer : singers) {
            const auto packageId = toQString(singer.packageId);
            const auto packageVersion = VersionUtils::stdc_to_qt(singer.packageVersion);
            const auto key = packageId + QLatin1Char('@') + packageVersion.toString();

            SingerInfo singerInfo(
                SingerIdentifier{toQString(singer.contributionId), packageId, packageVersion},
                toQString(singer.name.text()), speakersOf(singer.capabilities),
                languagesOf(singer.capabilities),
                toQString(singer.capabilities.defaultLanguage));
            singerInfo.setLocalizedNames(toLocalizedTextMap(singer.name));
            singerInfo.setCapability(summaryOf(singer.capabilities));
            // Spec 2.4 permits optional stages, but the editor currently runs all five stages.
            const auto &stages = singer.capabilities;
            const bool complete = stages.supportsEditorPipeline();
            const bool addressable = !stages.speakers.empty() || stages.unaddressableSpeakers.empty();
            const bool usable = complete && addressable;
            singerInfo.setResolutionState(usable ? ResolutionState::Resolved
                                                 : ResolutionState::Missing);
            if (!complete)
                qWarning() << "Singer" << toQString(singer.contributionId) << "in" << packageId
                           << "is missing stages required by the editor inference pipeline";
            // Report declared speakers that could not be mapped to the acoustic model.
            for (const auto &id : singer.capabilities.unaddressableSpeakers) {
                qWarning() << "Speaker" << toQString(id) << "of singer"
                           << toQString(singer.contributionId) << "in" << packageId
                           << "is not mapped to a speaker of the acoustic model and is not offered";
            }
            QSet<QString> convertibleLanguages;
            for (const auto &language : singer.convertibleLanguages)
                convertibleLanguages.insert(toQString(language));
            result.convertibleLanguages.insert(singerInfo.identifier(),
                                               std::move(convertibleLanguages));
            packages[packageAt.value(key)].addSinger(singerInfo);
        }
        result.successfulPackages = std::move(packages);

        qDebug() << "Package scan completed in" << timer.elapsed() << "ms";
        {
            QWriteLocker writeLocker(&m_resultRwLock);
            m_result = result;
            ++m_catalogGeneration;
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
    };

    // The scan opens packages and reads their files, so it can throw. Automation runs this refresh
    // on a bare QThreadPool, where an escaping exception terminates the process, and the
    // bookkeeping below would never clear m_refreshing, which leaves every later refresh waiting
    // forever. Converting the exception into a failed refresh keeps both properties intact.
    Expected<GetInstalledPackagesResult, GetInstalledPackagesError> completed =
        [&runScan]() -> Expected<GetInstalledPackagesResult, GetInstalledPackagesError> {
        try {
            return runScan();
        } catch (const std::exception &error) {
            return GetInstalledPackagesError{GetInstalledPackagesErrorType::Unknown,
                                             QString::fromUtf8(error.what())};
        } catch (...) {
            return GetInstalledPackagesError{GetInstalledPackagesErrorType::Unknown,
                                             QStringLiteral("Unknown error")};
        }
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
        // Emit from the manager's thread so UI receivers observe signals in order.
        QMetaObject::invokeMethod(
            this,
            [this, refreshedPackages = std::move(refreshedPackages)] {
                Q_EMIT packagesRefreshed(refreshedPackages);
            },
            Qt::QueuedConnection);
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
