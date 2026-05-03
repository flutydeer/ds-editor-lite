#include "OnsetMarkerMgr.h"

#include "MandarinOnsetMarker.h"
#include "RuleBasedOnsetMarker.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

LITE_SINGLETON_IMPLEMENT_INSTANCE(OnsetMarkerMgr)

OnsetMarkerMgr::OnsetMarkerMgr() {
    registerMarker("cmn", new MandarinOnsetMarker);
    loadPhonemeConfigs();
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

void OnsetMarkerMgr::loadPhonemeConfigs() {
    const auto dir =
        QDir(QCoreApplication::applicationDirPath() + "/Resources/phoneme");
    if (!dir.exists())
        return;
    const auto entries = dir.entryInfoList({"*.json"}, QDir::Files);
    for (const auto &entry : entries) {
        const auto lang = entry.baseName();
        if (m_markers.contains(lang))
            continue;
        if (loadRuleBasedMarker(lang, entry.absoluteFilePath()))
            qInfo() << "OnsetMarkerMgr: loaded phoneme config for" << lang;
    }
}
