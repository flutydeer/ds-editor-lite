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
    /// Copies all translations of a synthrt DisplayText into a Qt map (language tag -> text, keys
    /// kept verbatim per ds-spec 2.4) so the host can re-resolve the display text per UI language
    /// without rescanning the voicebank.
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

    /// What the host can say about a singer's capabilities on this line.
    ///
    /// The older line derived this by re-reading each model's configuration file by hand and
    /// reconciling what the four of them said. None of that happens here: a package that loaded
    /// has already had its models interpreted and its imports validated, so a capability is read
    /// off what the models export and cannot disagree with them.
    ///
    /// Three of the older fields have no answer on this line and are left at their "not known"
    /// value rather than invented. `vocoderPitchControllable` is a vocoder configuration key that
    /// nothing exports; `effectivePhonemes` was an intersection of the four phoneme tables, and
    /// only feeds a change-detection hash; the consistency levels described a reconciliation that
    /// no longer happens, because a voicebank whose models disagree does not load at all.
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
                // The string pair is the serialised form and is kept in step with the numbers.
                info.setToneMin(QString::number(speaker.toneRange->first));
                info.setToneMax(QString::number(speaker.toneRange->second));
            }
            // Every speaker the acoustic model accepts can be mixed with any other: they are all
            // that model's, so there is no second list to be absent from.
            info.setMixable(true);
            result.append(std::move(info));
        }
        return result;
    }

    QList<LanguageInfo> languagesOf(const lite::synthrt::SingerCapabilities &capabilities) {
        QList<LanguageInfo> result;
        result.reserve(static_cast<qsizetype>(capabilities.languages.size()));
        for (const auto &handle : capabilities.languages) {
            // Only the handle. What the older line also carried here -- a G2P identifier, a
            // dictionary path, an s2p and an onset mode -- described that line's own G2P system.
            // On this one a language is a linguist contribution and those four are its internals,
            // not the singer's, so the singer no longer states them and this no longer shows them.
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

        // The engine is brought up asynchronously by InferEngine, so the first scan may arrive
        // before it exists. Waiting here rather than failing is what makes a package list at
        // startup show the packages rather than an error that goes away by itself.
        //
        // There is one stage now. The older line split initialisation in two so that a package
        // scan did not have to wait on model loading; on this line nothing is loaded until a
        // synthesis asks for it, so there is no second stage to skip.
        if (!SynthrtEngine::instance().initializationDone()) {
            if (!SynthrtEngine::instance().waitForInitialization()) {
                return GetInstalledPackagesError{
                    GetInstalledPackagesErrorType::MetadataBackendNotInitialized,
                    QStringLiteral("SynthrtEngine initialization timed out"),
                };
            }
        }

        std::vector<lite::synthrt::PackageProblem> problems;
        auto scanned = SynthrtEngine::instance().refreshVoicebanks(searchPaths, &problems);
        if (!scanned) {
            return GetInstalledPackagesError{
                GetInstalledPackagesErrorType::MetadataBackendNotInitialized,
                QString::fromStdString(scanned.error().message()),
            };
        }
        const auto singers = scanned.take();

        // A package that would not open is shown with its reason rather than silently missing:
        // someone who installed a voicebank and cannot see it needs to be told why.
        for (const auto &problem : problems) {
            result.failedPackages.emplace_back(StringUtils::path_to_qstr(problem.path),
                                               QString::fromStdString(problem.reason));
        }

        // Singers arrive one per contribution; a package is what holds them. Grouping by identity
        // rather than by path because two directories may hold the same package and the loader
        // has already decided which one won.
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
            // A singer in the catalogue is a singer that loaded, imports and all. The older line
            // needed three states here because it listed singers it had not finished resolving.
            singerInfo.setResolutionState(ResolutionState::Resolved);
            packages[packageAt.value(key)].addSinger(singerInfo);
        }
        result.successfulPackages = std::move(packages);

        qDebug() << "Package scan completed in" << timer.elapsed() << "ms";
        if (commitGate && !commitGate()) {
            commitRejected = true;
            return result;
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

QString PackageManager::srtErrorToString(const srt::Error &error) {
    if (error.ok()) {
        return tr("No error");
    }
    // The whole chain rather than this error's own text: an error here is usually raised several
    // layers down -- a model that would not open, under an import that would not resolve, under a
    // package that would not load -- and only the innermost one says anything useful.
    return QString::fromStdString(error.toString());
}
