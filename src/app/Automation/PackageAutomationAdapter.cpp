#include "PackageAutomationAdapter.h"

#include <diffsinger/Bank/PackageValidator.h>

#include <lite/PackageManager/PackageManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/Support/StringUtils.h>

namespace Automation {
    namespace {
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
    }

    PackageRuntimeServices createPackageAutomationServices(PackageManager *manager) {
        PackageRuntimeServices services;
        if (!manager)
            return services;
        services.installedPackages = [manager] {
            QList<PackageDto> result;
            const auto packages = manager->installedPackages();
            for (const auto &package : packages.successfulPackages) {
                PackageDto converted{
                    .id = package.id(),
                    .version = package.version(),
                    .vendor = package.vendor(),
                    .description = package.description(),
                    .license = package.license(),
                    .readme = package.readme(),
                    .url = package.url(),
                    .path = package.path(),
                };
                for (const auto &singer : package.singers()) {
                    converted.singers.append({
                        .singerId = singer.singerId(),
                        .packageId = singer.packageId(),
                        .packageVersion = singer.packageVersion(),
                        .name = singer.name(),
                    });
                }
                result.append(std::move(converted));
            }
            return result;
        };
        services.validatePackage = [](const QString &path) {
            ds::bank::PackageValidator validator;
            const auto report = validator.validatePackage(
                StringUtils::qstr_to_path(path), ds::bank::PackageValidator::SchemaVersion::V10);
            PackageValidationReportDto result;
            result.hasErrors = report.hasErrors();
            for (const auto &item : report.items()) {
                PackageValidationSeverity severity = PackageValidationSeverity::Info;
                if (item.severity == ds::bank::ValidationItem::Warning)
                    severity = PackageValidationSeverity::Warning;
                else if (item.severity == ds::bank::ValidationItem::Error)
                    severity = PackageValidationSeverity::Error;
                result.items.append({
                    .severity = severity,
                    .path = QString::fromStdString(item.path),
                    .message = QString::fromStdString(item.message),
                    .actualValue = QString::fromStdString(item.actualValue),
                    .recommendation = QString::fromStdString(item.recommendation),
                });
            }
            return AutomationResult<PackageValidationReportDto>(std::move(result));
        };
        services.resolveDocumentVoices = [manager](AppModel *model, const bool apply) {
            return resolveDocumentVoices(manager, model, apply);
        };
        return services;
    }

} // namespace Automation
