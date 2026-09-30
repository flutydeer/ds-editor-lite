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

    /// Copies all translations of a synthrt DisplayText into a Qt map (language tag -> text, keys
    /// kept verbatim per ds-spec 2.4), so that the host can re-resolve the display text per UI
    /// language without rescanning the voicebank.
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

    /// Returns the capability summary of a singer, derived from the exports of its loaded models.
    ///
    /// The previous implementation re-read the configuration file of each model and reconciled the
    /// four files. The models of a loaded package have already been interpreted and its imports
    /// validated, so each capability is read from the model exports and cannot contradict them.
    ///
    /// Three former fields cannot be derived and keep their "unknown" value.
    /// `vocoderPitchControllable` is a vocoder configuration key that no model exports.
    /// `effectivePhonemes` was the intersection of the four phoneme tables and only feeds a
    /// change-detection hash. The consistency levels described a reconciliation that no longer
    /// takes place, because a voicebank with inconsistent models fails to load.
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
                // The string pair is the serialized form and is kept consistent with the numbers.
                info.setToneMin(QString::number(speaker.toneRange->first));
                info.setToneMax(QString::number(speaker.toneRange->second));
            }
            // Every speaker that the acoustic model accepts can be mixed with every other speaker,
            // because all of them belong to that model and no separate list of mixable speakers
            // exists.
            info.setMixable(true);
            result.append(std::move(info));
        }
        return result;
    }

    QList<LanguageInfo> languagesOf(const lite::synthrt::SingerCapabilities &capabilities) {
        QList<LanguageInfo> result;
        result.reserve(static_cast<qsizetype>(capabilities.languages.size()));
        for (const auto &handle : capabilities.languages) {
            // Only the handle is set. The G2P identifier, dictionary path, S2P and onset mode of
            // the previous implementation belonged to its own G2P system. A language is now a
            // linguist contribution, and these four values are internals of the linguist rather
            // than properties of the singer, so the singer does not declare them and they are not
            // listed. The handle also serves as the display name until the language layer
            // supplies a display name. The G2P fields of LanguageInfo remain empty.
            result.append(LanguageInfo(toQString(handle), toQString(handle)));
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
                                             RefreshCommitGate commitGate, const ScanMode mode) {
    {
        std::unique_lock lock(m_refreshMutex);
        if (m_refreshing) {
            qDebug() << "Already refreshing, wait for completion";
            m_refreshCompleted.wait(lock, [this] { return !m_refreshing; });
            if (!m_lastRefreshCommitRejected) {
                auto result = m_lastRefreshResult;
                lock.unlock();
                // The leading refresh has already committed, and this caller shares its result.
                // The gate still determines whether the caller may use the result, so a caller
                // whose request is no longer current receives the rejection rather than the
                // declined result.
                if (result && commitGate && !commitGate())
                    return commitRejectedError();
                return result;
            }
            qDebug() << "Leading package refresh was not committed, retry scan";
        }
        m_refreshing = true;
    }

    bool commitRejected = false;
    auto completed =
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

        // InferEngine initializes the engine asynchronously, so the first scan may start before
        // the engine exists. The scan waits instead of failing, so that a package list at startup
        // shows the packages rather than a transient error.
        //
        // Initialization has one stage. The previous implementation split initialization into
        // two stages so that a package scan did not wait for model loading. Models are now loaded
        // only when a synthesis requires them, so no second stage exists.
        if (!SynthrtEngine::instance().initializationDone()) {
            if (!SynthrtEngine::instance().waitForInitialization()) {
                return GetInstalledPackagesError{
                    GetInstalledPackagesErrorType::MetadataBackendNotInitialized,
                    QStringLiteral("SynthrtEngine initialization timed out"),
                };
            }
        }

        std::vector<lite::synthrt::PackageProblem> problems;
        auto scanned = SynthrtEngine::instance().refreshVoicebanks(
            searchPaths, &problems,
            mode == ScanMode::ReuseLoaded ? SynthrtEngine::RescanMode::ReuseLoaded
                                          : SynthrtEngine::RescanMode::Reload);
        if (!scanned) {
            return GetInstalledPackagesError{
                GetInstalledPackagesErrorType::MetadataBackendNotInitialized,
                QString::fromStdString(scanned.error().message()),
            };
        }
        const auto singers = scanned.take();

        // A package that failed to open is listed with its reason rather than omitted, so that a
        // user who installed the voicebank sees the cause of the failure.
        for (const auto &problem : problems) {
            result.failedPackages.emplace_back(StringUtils::path_to_qstr(problem.path),
                                               QString::fromStdString(problem.reason));
        }

        // Singers are reported per contribution and grouped into their packages. The grouping
        // uses the package identity rather than the path, because two directories may contain
        // the same package and the loader has already selected one of the two directories.
        QList<PackageInfo> packages;
        QHash<QString, qsizetype> packageAt;
        for (const auto &singer : singers) {
            const auto packageId = toQString(singer.packageId);
            const auto packageVersion = VersionUtils::stdc_to_qt(singer.packageVersion);
            const auto key = packageId + QLatin1Char('@') + packageVersion.toString();

            if (!packageAt.contains(key)) {
                PackageInfo packageInfo(packageId, packageVersion,
                                        toQString(singer.packageVendor.text()),
                                        toQString(singer.packageDescription.text()),
                                        toQString(singer.packageCopyright.text()), {}, {},
                                        StringUtils::path_to_qstr(singer.packagePath));
                packageInfo.setLocalizedVendor(toLocalizedTextMap(singer.packageVendor));
                packageInfo.setLocalizedDescription(toLocalizedTextMap(singer.packageDescription));
                packageInfo.setLocalizedLicense(toLocalizedTextMap(singer.packageCopyright));
                packageAt.insert(key, packages.size());
                packages.append(std::move(packageInfo));
            }

            SingerInfo singerInfo(
                SingerIdentifier{toQString(singer.contributionId), packageId, packageVersion},
                toQString(singer.name.text()), speakersOf(singer.capabilities),
                languagesOf(singer.capabilities),
                toQString(singer.capabilities.defaultLanguage));
            singerInfo.setLocalizedNames(toLocalizedTextMap(singer.name));
            singerInfo.setCapability(summaryOf(singer.capabilities));
            // Every singer in the catalog has loaded, including all of its imports. The previous
            // implementation needed three states because it listed partially resolved singers.
            singerInfo.setResolutionState(ResolutionState::Resolved);
            // Speakers that the acoustic model cannot address are excluded from the singer rather
            // than offered and rejected later, so they are reported here. Without this warning, a
            // voicebank with this defect is indistinguishable from a voicebank with fewer speakers.
            for (const auto &id : singer.capabilities.unaddressableSpeakers) {
                qWarning() << "Speaker" << toQString(id) << "of singer"
                           << toQString(singer.contributionId) << "in" << packageId
                           << "is not mapped to a speaker of the acoustic model and is not offered";
            }
            packages[packageAt.value(key)].addSinger(singerInfo);
        }
        result.successfulPackages = std::move(packages);

        qDebug() << "Package scan completed in" << timer.elapsed() << "ms";
        if (commitGate && !commitGate()) {
            commitRejected = true;
            return commitRejectedError();
        }
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
        // A refresh runs on a worker thread, and the receivers of this signal are widgets and
        // controllers of the application thread. The signal is emitted from the thread of the
        // manager, so that every receiver observes it in a consistent order relative to the other
        // signals of the manager and no receiver is invoked on the worker thread.
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

QString PackageManager::srtErrorToString(const srt::Error &error) {
    if (error.ok()) {
        return tr("No error");
    }
    // Returns the whole error chain rather than the text of this error alone. An error here
    // usually originates several layers down, for example a model that failed to open under an
    // import that failed to resolve under a package that failed to load, and only the innermost
    // error states the specific cause.
    return QString::fromStdString(error.toString());
}
