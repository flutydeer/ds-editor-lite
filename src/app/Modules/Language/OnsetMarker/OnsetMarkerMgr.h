#ifndef ONSETMARKERMGR_H
#define ONSETMARKERMGR_H

#include "IOnsetMarker.h"

#include "Utils/Singleton.h"
#include "Modules/Inference/Models/SingerIdentifier.h"

#include <QHash>
#include <QString>

class OnsetMarkerMgr {
private:
    OnsetMarkerMgr();
    ~OnsetMarkerMgr();

public:
    LITE_SINGLETON_DECLARE_INSTANCE(OnsetMarkerMgr)
    Q_DISABLE_COPY_MOVE(OnsetMarkerMgr)

    void registerMarker(const SingerIdentifier &singerIdentifier, const QString &language,
                        IOnsetMarker *marker);
    bool addOnsetMarker(const SingerIdentifier &singerIdentifier, const QString &language,
                        const QString &mode, const QString &filePath);
    bool loadRuleBasedMarker(const SingerIdentifier &singerIdentifier, const QString &language,
                             const QString &configPath);
    const IOnsetMarker *marker(const SingerIdentifier &singerIdentifier,
                               const QString &language) const;

private:
    static QString markerKey(const SingerIdentifier &singerIdentifier, const QString &language);

    QHash<QString, IOnsetMarker *> m_markers;
};

#endif // ONSETMARKERMGR_H
