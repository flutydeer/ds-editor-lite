#include "LanguageOption.h"

#include <QJsonArray>
#include <language-manager/IG2pManager.h>

#include <language-manager/ILanguageManager.h>

void LanguageOption::load(const QJsonObject &object) {
    if (object.contains("langOrder")) {
        langOrder = object.value("langOrder").toVariant().toStringList();
        if (!langOrder.isEmpty())
            LangMgr::ILanguageManager::instance()->setDefaultOrder(langOrder);
    }

    if (object.contains("langConfigs")) {
        const auto langMgr = LangMgr::ILanguageManager::instance();
        langConfigs = object.value("langConfigs").toObject();
        for (const auto &key : langOrder) {
            if (langConfigs.contains(key)) {
                if (const auto langFactory = langMgr->language(key)) {
                    langFactory->loadConfig(langConfigs.value(key).toObject());
                }
            }
        }
    }

    if (object.contains("g2pConfigs")) {
        const auto g2pMgr = LangMgr::IG2pManager::instance();
        g2pConfigs = object.value("g2pConfigs").toObject();
        for (const auto &key : langOrder) {
            if (g2pConfigs.contains(key)) {
                if (const auto g2pFactory = g2pMgr->g2p(key)) {
                    g2pFactory->loadConfig(g2pConfigs.value(key).toObject());
                }
            }
        }
    }
}

void LanguageOption::save(QJsonObject &object) {
    object.insert("langOrder", QJsonArray::fromStringList(langOrder));
    QJsonObject langOptionsObject;
    const auto langMgr = LangMgr::ILanguageManager::instance();
    for (const auto &key : langOrder) {
        if (const auto langFactory = langMgr->language(key)) {
            langOptionsObject.insert(key, langFactory->exportConfig());
        }
    }
    object.insert("g2pConfigs", langOptionsObject);
}