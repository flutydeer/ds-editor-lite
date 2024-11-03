#include "LanguageOption.h"

#include <QJsonArray>

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

    if (object.contains("g2pConfigs")) {
        const auto langMgr = LangMgr::ILanguageManager::instance();
        g2pConfigs = object.value("g2pConfigs").toObject();
        for (const auto &key : langOrder) {
            if (g2pConfigs.contains(key)) {
                if (const auto g2pFactory = langMgr->g2p(key)) {
                    g2pFactory->loadAllConfig(g2pConfigs.value(key).toObject());
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
        if (const auto g2pFactory = langMgr->g2p(key)) {
            langOptionsObject.insert(key, g2pFactory->allConfig());
        }
    }
    object.insert("g2pConfigs", langOptionsObject);
}