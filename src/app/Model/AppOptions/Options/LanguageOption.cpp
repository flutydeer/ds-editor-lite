#include "LanguageOption.h"

#include <QJsonArray>

void LanguageOption::load(const QJsonObject &object) {
    if (object.contains("langOrder")) {
        langOrder = object.value("langOrder").toVariant().toStringList();
    }
}

void LanguageOption::save(QJsonObject &object) {
    object.insert("langOrder", QJsonArray::fromStringList(langOrder));
}