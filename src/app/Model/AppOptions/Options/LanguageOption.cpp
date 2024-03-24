#include "LanguageOption.h"

#include <QJsonArray>

#include "Modules/Language/LangMgr/ILanguageManager.h"

void LanguageOption::load(const QJsonObject &object) {
    if (object.contains("langOrder")) {
        langOrder = object.value("langOrder").toVariant().toStringList();
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
    object.insert("langConfigs", langOptionsObject);
}