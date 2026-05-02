#include "OnsetMarkerMgr.h"

#include "MandarinOnsetMarker.h"

LITE_SINGLETON_IMPLEMENT_INSTANCE(OnsetMarkerMgr)

OnsetMarkerMgr::OnsetMarkerMgr() {
    registerMarker("cmn", new MandarinOnsetMarker);
}

OnsetMarkerMgr::~OnsetMarkerMgr() {
    qDeleteAll(m_markers);
}

void OnsetMarkerMgr::registerMarker(const QString &language, IOnsetMarker *marker) {
    m_markers.insert(language, marker);
}

const IOnsetMarker *OnsetMarkerMgr::marker(const QString &language) const {
    if (const auto it = m_markers.find(language); it != m_markers.end())
        return it.value();
    return &m_default;
}
