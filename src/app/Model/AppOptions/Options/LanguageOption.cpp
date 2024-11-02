#include "LanguageOption.h"

#include <QJsonArray>
#include <language-manager/IG2pManager.h>

#include <language-manager/ILanguageManager.h>

static QPair<int, QString> extractLeadingNumber(const QString &dataString) {
    static QRegularExpression re(R"(^(\d+))");
    const QRegularExpressionMatch match = re.match(dataString);

    if (match.hasMatch()) {
        int number = match.captured(1).toInt();
        QString remaining =
            dataString.mid(match.capturedLength(1)); // Get the remaining part of the string
        return qMakePair(number, remaining);
    }
    return qMakePair(-1, dataString); // Return -1 and the original string if no number is found
}

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
        for (const auto &configG2pId : g2pConfigs.keys()) {
            const auto [g2pNum, g2pType] = extractLeadingNumber(configG2pId);
            if (g2pNum >= 0) {
                // const auto oldG2p = g2pMgr->g2p(g2pType);
                // if (oldG2p == nullptr)
                //     return;
                //
                // const auto addNewG2p = g2pMgr->addG2p(
                //     oldG2p->clone(configG2pId, oldG2p->category(), oldG2p->parent()));
                // if (addNewG2p)
                //     g2pMgr->g2p(configG2pId)->loadConfig(g2pConfigs.value(configG2pId).toObject());
            } else
                g2pMgr->g2p(configG2pId)->loadConfig(g2pConfigs.value(configG2pId).toObject());
        }
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