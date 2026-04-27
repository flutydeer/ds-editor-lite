#include "FillLyricOption.h"

#include <QJsonArray>

// CustomSplitterRule serialization

QJsonObject CustomSplitterRule::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["regexes"] = QJsonArray::fromStringList(regexes);
    obj["enabled"] = enabled;
    obj["order"] = order;
    return obj;
}

CustomSplitterRule CustomSplitterRule::fromJson(const QJsonObject &obj) {
    CustomSplitterRule rule;
    rule.name = obj["name"].toString();
    rule.enabled = obj.value("enabled").toBool(true);
    rule.order = obj.value("order").toInt(0);
    if (obj.contains("regexes") && obj["regexes"].isArray()) {
        for (const auto &v : obj["regexes"].toArray()) {
            if (v.isString())
                rule.regexes.append(v.toString());
        }
    }
    return rule;
}

// CustomTaggerEntry serialization

QJsonObject CustomTaggerEntry::toJson() const {
    QJsonObject obj;
    obj["type"] = type;
    obj["value"] = QJsonArray::fromStringList(value);
    obj["tag"] = tag;
    if (discard)
        obj["discard"] = true;
    return obj;
}

CustomTaggerEntry CustomTaggerEntry::fromJson(const QJsonObject &obj) {
    CustomTaggerEntry entry;
    entry.type = obj["type"].toString();
    entry.tag = obj["tag"].toString();
    entry.discard = obj.value("discard").toBool(false);
    if (obj.contains("value") && obj["value"].isArray()) {
        for (const auto &v : obj["value"].toArray()) {
            if (v.isString())
                entry.value.append(v.toString());
        }
    }
    return entry;
}

// CustomTaggerRule serialization

QJsonObject CustomTaggerRule::toJson() const {
    QJsonObject obj;
    obj["language"] = language;
    obj["enabled"] = enabled;
    QJsonArray arr;
    for (const auto &e : entries)
        arr.append(e.toJson());
    obj["tagger"] = arr;
    return obj;
}

CustomTaggerRule CustomTaggerRule::fromJson(const QJsonObject &obj) {
    CustomTaggerRule rule;
    rule.language = obj["language"].toString();
    rule.enabled = obj.value("enabled").toBool(true);
    if (obj.contains("tagger") && obj["tagger"].isArray()) {
        for (const auto &v : obj["tagger"].toArray()) {
            if (v.isObject())
                rule.entries.append(CustomTaggerEntry::fromJson(v.toObject()));
        }
    }
    return rule;
}

// FillLyricOption

static QJsonObject mapToJson(const QMap<QString, bool> &map) {
    QJsonObject obj;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it)
        obj[it.key()] = it.value();
    return obj;
}

static QMap<QString, bool> jsonToMap(const QJsonObject &obj) {
    QMap<QString, bool> map;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        map[it.key()] = it.value().toBool(true);
    return map;
}

void FillLyricOption::load(const QJsonObject &object) {
    if (object.contains("baseVisible"))
        baseVisible = object["baseVisible"].toBool();
    if (object.contains("extVisible"))
        extVisible = object["extVisible"].toBool();

    if (object.contains("splitMode"))
        splitMode = object["splitMode"].toInt();
    if (object.contains("skipSlur"))
        skipSlur = object["skipSlur"].toBool();

    if (object.contains("exportLanguage"))
        exportLanguage = object["exportLanguage"].toBool();

    if (object.contains("textEditFontSize"))
        textEditFontSize = object["textEditFontSize"].toDouble();
    if (object.contains("viewFontSize"))
        viewFontSize = object["viewFontSize"].toDouble();

    if (object.contains("builtinSplitterEnabled"))
        builtinSplitterEnabled = jsonToMap(object["builtinSplitterEnabled"].toObject());
    if (object.contains("builtinTaggerEnabled"))
        builtinTaggerEnabled = jsonToMap(object["builtinTaggerEnabled"].toObject());

    if (object.contains("customSplitterRules") && object["customSplitterRules"].isArray()) {
        customSplitterRules.clear();
        for (const auto &v : object["customSplitterRules"].toArray()) {
            if (v.isObject())
                customSplitterRules.append(CustomSplitterRule::fromJson(v.toObject()));
        }
    }

    if (object.contains("customTaggerRules") && object["customTaggerRules"].isArray()) {
        customTaggerRules.clear();
        for (const auto &v : object["customTaggerRules"].toArray()) {
            if (v.isObject())
                customTaggerRules.append(CustomTaggerRule::fromJson(v.toObject()));
        }
    }

    if (object.contains("splitterOrder") && object["splitterOrder"].isArray()) {
        splitterOrder.clear();
        for (const auto &v : object["splitterOrder"].toArray()) {
            if (v.isString())
                splitterOrder.append(v.toString());
        }
    }

    if (object.contains("taggerOrder") && object["taggerOrder"].isArray()) {
        taggerOrder.clear();
        for (const auto &v : object["taggerOrder"].toArray()) {
            if (v.isString())
                taggerOrder.append(v.toString());
        }
    }
}

void FillLyricOption::save(QJsonObject &object) {
    object["baseVisible"] = baseVisible;
    object["extVisible"] = extVisible;

    object["splitMode"] = splitMode;
    object["skipSlur"] = skipSlur;

    object["exportLanguage"] = exportLanguage;

    object["textEditFontSize"] = textEditFontSize;
    object["viewFontSize"] = viewFontSize;

    object["builtinSplitterEnabled"] = mapToJson(builtinSplitterEnabled);
    object["builtinTaggerEnabled"] = mapToJson(builtinTaggerEnabled);

    QJsonArray splitterArr;
    for (const auto &r : customSplitterRules)
        splitterArr.append(r.toJson());
    object["customSplitterRules"] = splitterArr;

    QJsonArray taggerArr;
    for (const auto &r : customTaggerRules)
        taggerArr.append(r.toJson());
    object["customTaggerRules"] = taggerArr;

    object["splitterOrder"] = QJsonArray::fromStringList(splitterOrder);
    object["taggerOrder"] = QJsonArray::fromStringList(taggerOrder);
}
