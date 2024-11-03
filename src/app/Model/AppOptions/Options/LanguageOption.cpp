#include "LanguageOption.h"

#include <QJsonArray>

#include <language-manager/ILanguageManager.h>

void LanguageOption::load(const QJsonObject &object) {
    if (object.contains("langOrder")) {
        langOrder = object.value("langOrder").toVariant().toStringList();
        if (!langOrder.isEmpty())
            LangMgr::ILanguageManager::instance()->setDefaultOrder(langOrder);
        else
            langOrder = LangMgr::ILanguageManager::instance()->defaultOrder();
    }

    if (object.contains("g2pConfigs")) {
        g2pConfigs = object.value("g2pConfigs").toObject();
    }
}

void LanguageOption::save(QJsonObject &object) {
    object.insert("langOrder", QJsonArray::fromStringList(langOrder));
    QJsonObject langOptionsObject;
    const auto langMgr = LangMgr::ILanguageManager::instance();
    for (const auto &key : langOrder) {
        if (const auto g2pFactory = langMgr->g2p(key)) {
            langOptionsObject.insert(key, g2pFactory->config());
        }
    }
    object.insert("g2pConfigs", langOptionsObject);
}