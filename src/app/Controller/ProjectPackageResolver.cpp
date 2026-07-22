//
// Created by FlutyDeer on 2026/6/6.
//

#include "ProjectPackageResolver.h"

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppModel/Track.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/PackageManager/PackageManager.h"

#include <QDebug>
#include <QTimer>

ProjectPackageResolver::ProjectPackageResolver(QObject *parent) : QObject(parent) {
    connect(appModel, &AppModel::modelChanged, this, &ProjectPackageResolver::scheduleResolve);
    connect(packageManager, &PackageManager::packagesRefreshed, this,
            [this](const QList<PackageInfo> &) { scheduleResolve(); });
    connect(appStatus, &AppStatus::moduleStatusChanged, this,
            [this](const AppStatus::ModuleType module, const AppStatus::ModuleStatus status) {
                if (module == AppStatus::ModuleType::Package &&
                    status == AppStatus::ModuleStatus::Ready)
                    scheduleResolve();
            });
}

ProjectPackageResolver::~ProjectPackageResolver() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(ProjectPackageResolver)

void ProjectPackageResolver::scheduleResolve() {
    if (m_resolveScheduled)
        return;

    m_resolveScheduled = true;
    QTimer::singleShot(0, this, [this] {
        m_resolveScheduled = false;
        resolveProject();
    });
}

void ProjectPackageResolver::resolveProject() {
    if (appStatus->packageModuleStatus != AppStatus::ModuleStatus::Ready)
        return;

    int resolvedPairs = 0;
    for (const auto track : appModel->tracks()) {
        const auto trackSinger = resolveSinger(track->singerInfo());
        const auto trackSpeaker = resolveSpeaker(trackSinger, track->speakerInfo());
        if (trackSinger != track->singerInfo() || trackSpeaker != track->speakerInfo()) {
            track->setSingerAndSpeakerInfo(trackSinger, trackSpeaker);
            resolvedPairs++;
        }

        for (const auto clip : track->clips()) {
            if (clip->clipType() != IClip::Singing)
                continue;

            const auto singingClip = static_cast<SingingClip *>(clip);
            singingClip->setTrackVoiceContext(track->singerInfo(), track->speakerInfo(),
                                              track->speakerMixData());

            if (!singingClip->usesTrackVoiceContext()) {
                const auto ownSinger = resolveSinger(singingClip->ownSingerInfo());
                const auto ownSpeaker = resolveSpeaker(ownSinger, singingClip->ownSpeakerInfo());
                if (ownSinger != singingClip->ownSingerInfo() ||
                    ownSpeaker != singingClip->ownSpeakerInfo()) {
                    singingClip->setOwnSingerAndSpeaker(ownSinger, ownSpeaker);
                    resolvedPairs++;
                }
            }
        }
    }

    if (resolvedPairs > 0)
        qInfo() << "Resolved project singer/speaker pairs from package metadata:" << resolvedPairs;
}

SingerInfo ProjectPackageResolver::resolveSinger(const SingerInfo &singerInfo) {
    const auto identifier = singerInfo.identifier();
    if (identifier.isEmpty())
        return singerInfo;

    const auto resolved = packageManager->findSingerByIdentifier(identifier);
    if (!resolved.isEmpty())
        return resolved; // 命中 → Resolved（PM 构造默认状态）

    // PM 已就绪（resolveProject 仅在 packageModuleStatus==Ready 时调用）但未命中 → Missing
    // 显式标记 Missing，区分"待补全"与"声库未安装"
    if (singerInfo.resolutionState() != ResolutionState::Missing) {
        SingerInfo missing = singerInfo;
        missing.setResolutionState(ResolutionState::Missing);
        return missing;
    }
    return singerInfo;
}

SpeakerInfo ProjectPackageResolver::resolveSpeaker(const SingerInfo &singerInfo,
                                                   const SpeakerInfo &speakerInfo) {
    if (speakerInfo.isEmpty())
        return {};

    for (const auto &speaker : singerInfo.speakers()) {
        if (speaker.id() == speakerInfo.id())
            return speaker;
    }
    return speakerInfo;
}
