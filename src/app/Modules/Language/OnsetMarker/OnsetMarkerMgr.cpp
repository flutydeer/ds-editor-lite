#include "OnsetMarkerMgr.h"

#include "MandarinOnsetMarker.h"
#include "RuleBasedOnsetMarker.h"

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

bool OnsetMarkerMgr::loadRuleBasedMarker(const QString &language, const QString &configPath) {
    auto marker = new RuleBasedOnsetMarker;
    if (!marker->loadConfig(configPath)) {
        delete marker;
        return false;
    }
    registerMarker(language, marker);
    return true;
}

const IOnsetMarker *OnsetMarkerMgr::marker(const QString &language) const {
    if (const auto it = m_markers.find(language); it != m_markers.end())
        return it.value();
    return &m_default;
}
