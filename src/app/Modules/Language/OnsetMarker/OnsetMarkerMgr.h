#ifndef ONSETMARKERMGR_H
#define ONSETMARKERMGR_H

#include "DefaultOnsetMarker.h"

#include "Utils/Singleton.h"

#include <QHash>

class OnsetMarkerMgr {
private:
    OnsetMarkerMgr();
    ~OnsetMarkerMgr();

public:
    LITE_SINGLETON_DECLARE_INSTANCE(OnsetMarkerMgr)
    Q_DISABLE_COPY_MOVE(OnsetMarkerMgr)

    void registerMarker(const QString &language, IOnsetMarker *marker);
    const IOnsetMarker *marker(const QString &language) const;

private:
    QHash<QString, IOnsetMarker *> m_markers;
    DefaultOnsetMarker m_default;
};

#endif // ONSETMARKERMGR_H
