#ifndef DSSP_METADATA_H
#define DSSP_METADATA_H

#include "DsspApi.h"

#include <memory>

namespace ds::session {
    struct VoicebankSnapshot;
}
namespace ds::bank {
    struct SingerSnapshot;
    struct SingerRef;
}

namespace DsspMetadata {

    /// Singer API id form: "packageId@version[singerId]" (dspx-compatible).
    struct SingerReference {
        QString packageId;
        QString version;
        QString singerId;
    };

    bool parseSingerId(const QString &apiId, SingerReference &out);
    QString encodeSingerId(const SingerReference &ref);

    /// Current immutable voicebank snapshot (thread-safe publication).
    std::shared_ptr<const ds::session::VoicebankSnapshot> currentSnapshot();

    /// Resolve a singer by (packageId, version, singerId) from the snapshot.
    const ds::bank::SingerSnapshot *findSinger(const SingerReference &ref);
    const ds::bank::SingerSnapshot *findSingerByApiId(const QString &apiId, SingerReference &out);

    DsspApi::Result applicationInfo();
    DsspApi::Result architectureList();
    DsspApi::Result architecture(const QString &archId);
    DsspApi::Result singerList();
    DsspApi::Result archSingerList(const QString &archId);
    DsspApi::Result singer(const QString &singerId);
    DsspApi::Result singerAvatar(const QString &singerId);
    DsspApi::Result singerBackground(const QString &singerId);
    DsspApi::Result singerDemoAudioList(const QString &singerId);
    DsspApi::Result envTag(const QJsonObject &body);

} // namespace DsspMetadata

#endif // DSSP_METADATA_H
