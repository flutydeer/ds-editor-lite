#include "PackageAutomationAdapter.h"

#include "Model/AppOptions/AppOptions.h"

#include <lite/SynthrtEngine/SynthrtEngine.h>

#include <lite/PackageManager/PackageManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/Support/StringUtils.h>

#include <QCoreApplication>
#include <QMap>
#include <QMetaObject>
#include <QPointer>
#include <QThreadPool>

#include <algorithm>
#include <memory>

namespace Automation {
    namespace {
        /// Both failure lists in this file pass through the loader's error text unchanged. The
        /// loader is the only component with the rejection reason, and a reworded reason could
        /// be inaccurate.
        PackageRefreshFailureDto failureDto(
            const GetInstalledPackagesResult::FailedPackage &failure) {
            return {
                .path = failure.path,
                .reason = failure.reason,
            };
        }

        SingerInfo resolveSinger(PackageManager *manager, const SingerInfo &singerInfo) {
            const auto identifier = singerInfo.identifier();
            if (identifier.isEmpty())
                return singerInfo;
            const auto resolved = manager->findSingerByIdentifier(identifier);
            if (!resolved.isEmpty())
                return resolved;
            if (singerInfo.resolutionState() == ResolutionState::Missing)
                return singerInfo;
            auto missing = singerInfo;
            missing.setResolutionState(ResolutionState::Missing);
            return missing;
        }

        SpeakerInfo resolveSpeaker(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo) {
            if (speakerInfo.isEmpty())
                return {};
            for (const auto &speaker : singerInfo.speakers()) {
                if (speaker.id() == speakerInfo.id())
                    return speaker;
            }
            return speakerInfo;
        }

        int resolveDocumentVoices(PackageManager *manager, AppModel *model, const bool apply) {
            if (!model)
                return 0;
            int resolvedPairs = 0;
            for (auto *track : model->tracks()) {
                const auto trackSinger = resolveSinger(manager, track->singerInfo());
                const auto trackSpeaker = resolveSpeaker(trackSinger, track->speakerInfo());
                if (trackSinger != track->singerInfo() || trackSpeaker != track->speakerInfo()) {
                    ++resolvedPairs;
                    if (apply)
                        track->setSingerAndSpeakerInfo(trackSinger, trackSpeaker);
                }
                const auto effectiveSinger = apply ? track->singerInfo() : trackSinger;
                const auto effectiveSpeaker = apply ? track->speakerInfo() : trackSpeaker;
                for (auto *clip : track->clips()) {
                    if (clip->clipType() != IClip::Singing)
                        continue;
                    auto *singingClip = static_cast<SingingClip *>(clip);
                    if (apply) {
                        singingClip->setTrackVoiceContext(effectiveSinger, effectiveSpeaker,
                                                          track->speakerMixData());
                    }
                    if (singingClip->usesTrackVoiceContext())
                        continue;
                    const auto ownSinger = resolveSinger(manager, singingClip->ownSingerInfo());
                    const auto ownSpeaker =
                        resolveSpeaker(ownSinger, singingClip->ownSpeakerInfo());
                    if (ownSinger != singingClip->ownSingerInfo() ||
                        ownSpeaker != singingClip->ownSpeakerInfo()) {
                        ++resolvedPairs;
                        if (apply)
                            singingClip->setOwnSingerAndSpeaker(ownSinger, ownSpeaker);
                    }
                }
            }
            return resolvedPairs;
        }

        QList<PackageDto> convertPackages(const GetInstalledPackagesResult &packages) {
            QList<PackageDto> result;
            for (const auto &package : packages.successfulPackages) {
                PackageDto converted{
                    .id = package.id(),
                    .version = package.version(),
                    .vendor = package.vendor(),
                    .localizedVendor = package.localizedVendor(),
                    .description = package.description(),
                    .localizedDescription = package.localizedDescription(),
                    .license = package.license(),
                    .localizedLicense = package.localizedLicense(),
                    .readme = package.readme(),
                    .url = package.url(),
                    .path = package.path(),
                };
                for (const auto &singer : package.singers()) {
                    const auto identifier = singer.identifier();
                    QSet<QString> convertibleLanguages;
                    for (const auto &language : singer.languages()) {
                        if (SynthrtEngine::instance().canConvert(identifier, language.id()))
                            convertibleLanguages.insert(language.id());
                    }
                    converted.singers.append({
                        .singerId = singer.singerId(),
                        .packageId = singer.packageId(),
                        .packageVersion = singer.packageVersion(),
                        .name = singer.name(),
                        .info = singer,
                        .convertibleLanguages = std::move(convertibleLanguages),
                    });
                }
                result.append(std::move(converted));
            }
            return result;
        }

        QString packageKey(const PackageDto &package) {
            return package.id + u'@' + package.version.toString();
        }

        PackageRefreshResultDto buildRefreshResult(const QList<PackageDto> &before,
                                                   const GetInstalledPackagesResult &afterRaw) {
            const auto after = convertPackages(afterRaw);
            QMap<QString, PackageDto> beforeByKey;
            QMap<QString, PackageDto> afterByKey;
            for (const auto &package : before)
                beforeByKey.insert(packageKey(package), package);
            for (const auto &package : after)
                afterByKey.insert(packageKey(package), package);

            PackageRefreshResultDto result{.packages = static_cast<int>(after.size())};
            for (auto it = afterByKey.cbegin(); it != afterByKey.cend(); ++it) {
                const auto previous = beforeByKey.constFind(it.key());
                if (previous == beforeByKey.cend())
                    result.added.append(it.key());
                else if (*previous != it.value())
                    result.updated.append(it.key());
            }
            for (auto it = beforeByKey.cbegin(); it != beforeByKey.cend(); ++it) {
                if (!afterByKey.contains(it.key()))
                    result.removed.append(it.key());
            }
            for (const auto &failure : afterRaw.failedPackages)
                result.failures.append(failureDto(failure));
            return result;
        }

        AutomationError packageRefreshError(const GetInstalledPackagesError &error) {
            AutomationError result;
            result.code = error.type == GetInstalledPackagesErrorType::MetadataBackendNotInitialized
                              ? AutomationErrorCode::ModuleNotReady
                              : AutomationErrorCode::IoError;
            result.message =
                error.type == GetInstalledPackagesErrorType::MetadataBackendNotInitialized
                    ? QStringLiteral("Package metadata service is unavailable")
                    : QStringLiteral("Package refresh failed");
            return result;
        }

        void deliverRefreshResult(PackageRefreshCompletion completion,
                                  AutomationResult<PackageRefreshResultDto> result) {
            if (auto *application = QCoreApplication::instance()) {
                QMetaObject::invokeMethod(
                    application,
                    [completion = std::move(completion), result = std::move(result)]() mutable {
                        completion(std::move(result));
                    },
                    Qt::QueuedConnection);
                return;
            }
            completion(std::move(result));
        }
    }

    PackageRuntimeServices createPackageAutomationServices(PackageManager *manager,
                                                           AppOptions *options) {
        PackageRuntimeServices services;
        if (!manager)
            return services;
        const auto effectiveSearchPaths =
            options ? options->general()->packageSearchPaths : QStringList{};
        services.installedPackages = [manager] {
            return convertPackages(manager->installedPackages());
        };
        services.installedPackageFailures = [manager] {
            QList<PackageRefreshFailureDto> result;
            for (const auto &failure : manager->installedPackages().failedPackages)
                result.append(failureDto(failure));
            return result;
        };
        // Validation loads the package and releases it immediately. This check is stronger than
        // a schema check because every interpreter reads its configuration and every import
        // validator runs. The report is less detailed: the loader returns one error with its
        // causes rather than a list of findings with severities and recommendations, so a report
        // contains at most one item and never a warning.
        //
        // The reduced report detail is intentional. An itemized report requires the loader to
        // collect all findings instead of returning at the first error, which is a change to
        // synthrt and cannot be reconstructed by the host.
        services.validatePackage = [](const QString &path) {
            // Validation before the engine starts or after it shuts down returns a ModuleNotReady
            // error. unitIfReady() retrieves and checks the unit in one call, instead of testing an
            // initialization flag first and retrieving the unit afterwards, which would leave a
            // race window between the two calls.
            auto *unit = SynthrtEngine::instance().unitIfReady();
            if (unit == nullptr) {
                AutomationError error;
                error.code = AutomationErrorCode::ModuleNotReady;
                error.message = QStringLiteral("The synthesis engine is unavailable");
                return AutomationResult<PackageValidationReportDto>(std::move(error));
            }
            PackageValidationReportDto result;
            auto opened =
                unit->openPackage(StringUtils::qstr_to_path(path), srt::SynthUnit::Load);
            if (opened) {
                // Released immediately because validation does not keep the package loaded. A
                // voicebank in use by the editor is held by the catalog, not by this handle.
                opened.take().reset();
                return AutomationResult<PackageValidationReportDto>(std::move(result));
            }
            result.hasErrors = true;
            result.items.append({
                .severity = PackageValidationSeverity::Error,
                .path = path,
                .message = QString::fromStdString(opened.error().toString()),
                .actualValue = {},
                .recommendation = {},
            });
            return AutomationResult<PackageValidationReportDto>(std::move(result));
        };
        services.resolveDocumentVoices = [manager](AppModel *model, const bool apply) {
            return resolveDocumentVoices(manager, model, apply);
        };
        const auto refreshThreadPool = std::make_shared<QThreadPool>();
        services.refreshPackages =
            [manager = QPointer<PackageManager>(manager), effectiveSearchPaths, refreshThreadPool](
                PackageRefreshCommitGate commitGate,
                PackageRefreshCompletion completion) -> AutomationResult<AutomationUnit> {
            if (!completion) {
                return AutomationError::invalidArgument(
                    QStringLiteral("completion"),
                    QStringLiteral("Package refresh completion callback is missing"));
            }
            if (!manager)
                return packageRefreshError({GetInstalledPackagesErrorType::Unknown,
                                            QStringLiteral("Package manager is unavailable")});
            refreshThreadPool->start([manager, effectiveSearchPaths,
                                      commitGate = std::move(commitGate),
                                      completion = std::move(completion)]() mutable {
                if (!manager) {
                    deliverRefreshResult(
                        std::move(completion),
                        packageRefreshError({GetInstalledPackagesErrorType::Unknown,
                                             QStringLiteral("Package manager is unavailable")}));
                    return;
                }
                const auto before = convertPackages(manager->installedPackages());
                bool commitGateReached = false;
                bool commitAccepted = false;
                PackageManager::RefreshCommitGate managerCommitGate;
                if (commitGate) {
                    managerCommitGate = [&] {
                        commitGateReached = true;
                        commitAccepted = commitGate();
                        return commitAccepted;
                    };
                }
                const auto refreshed = manager->refreshInstalledPackages(
                    effectiveSearchPaths, std::move(managerCommitGate));
                if (commitGateReached && !commitAccepted)
                    return;
                if (!refreshed) {
                    deliverRefreshResult(std::move(completion),
                                         packageRefreshError(refreshed.getError()));
                    return;
                }
                deliverRefreshResult(std::move(completion),
                                     buildRefreshResult(before, refreshed.get()));
            });
            return AutomationUnit{};
        };
        return services;
    }

} // namespace Automation
