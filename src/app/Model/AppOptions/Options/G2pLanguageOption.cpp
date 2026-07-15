#include "G2pLanguageOption.h"

#include <QJsonArray>

void G2pLanguageOption::load(const QJsonObject &object) {
    if (object.contains("langOrder"))
        langOrder = object.value("langOrder").toVariant().toStringList();
}

void G2pLanguageOption::save(QJsonObject &object) {
    object.insert("langOrder", QJsonArray::fromStringList(langOrder));
}
