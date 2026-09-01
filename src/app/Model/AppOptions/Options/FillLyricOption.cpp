#include "FillLyricOption.h"

#include "Modules/FillLyric/Utils/LyricRuleAutomationUtils.h"

#include <lite/Support/JsonUtils.h>

#include <QJsonArray>
#include <QSet>

// CustomSplitterRule serialization

QJsonObject CustomSplitterRule::toJson() const {
    QJsonObject obj;
    obj["ruleId"] = ruleId;
    obj["name"] = name;
    obj["regexes"] = QJsonArray::fromStringList(regexes);
    obj["enabled"] = enabled;
    obj["order"] = order;
    return obj;
}

CustomSplitterRule CustomSplitterRule::fromJson(const QJsonObject &obj) {
    CustomSplitterRule rule;
    rule.ruleId = obj["ruleId"].toString();
    rule.name = obj["name"].toString();
    rule.enabled = obj.value("enabled").toBool(true);
    rule.order = obj.value("order").toInt(0);
    rule.regexes = JsonUtils::deserializeStringList(obj["regexes"].toArray());
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
    entry.value = JsonUtils::deserializeStringList(obj["value"].toArray());
    return entry;
}

// CustomTaggerRule serialization

QJsonObject CustomTaggerRule::toJson() const {
    return QJsonObject{
        {"ruleId",   ruleId                                 },
        {"name",     name                                   },
        {"language", language                               },
        {"enabled",  enabled                                },
        {"tagger",   JsonUtils::serializeListToJson(entries)}
    };
}

CustomTaggerRule CustomTaggerRule::fromJson(const QJsonObject &obj) {
    CustomTaggerRule rule;
    rule.ruleId = obj["ruleId"].toString();
    rule.language = obj["language"].toString();
    rule.name = obj["name"].toString();
    if (rule.name.isEmpty())
        rule.name = rule.language;
    rule.enabled = obj.value("enabled").toBool(true);
    rule.entries = JsonUtils::deserializeListFromJson<CustomTaggerEntry>(obj["tagger"].toArray());
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

    if (object.contains("customSplitterRules"))
        customSplitterRules = JsonUtils::deserializeListFromJson<CustomSplitterRule>(
            object["customSplitterRules"].toArray());

    if (object.contains("customTaggerRules"))
        customTaggerRules = JsonUtils::deserializeListFromJson<CustomTaggerRule>(
            object["customTaggerRules"].toArray());

    if (object.contains("splitterOrder"))
        splitterOrder = JsonUtils::deserializeStringList(object["splitterOrder"].toArray());

    if (object.contains("taggerOrder"))
        taggerOrder = JsonUtils::deserializeStringList(object["taggerOrder"].toArray());

    ensureStableRuleIds();
}

void FillLyricOption::ensureStableRuleIds() {
    QSet<QString> used;
    const auto normalize = [&used](QString &ruleId) {
        const auto valid = FillLyric::isAutomationRuleId(ruleId) &&
                           !ruleId.startsWith(QStringLiteral("builtin-")) && !used.contains(ruleId);
        if (!valid)
            ruleId = FillLyric::createAutomationRuleId();
        used.insert(ruleId);
    };
    for (auto &rule : customSplitterRules)
        normalize(rule.ruleId);
    for (auto &rule : customTaggerRules)
        normalize(rule.ruleId);
}

void FillLyricOption::save(QJsonObject &object) {
    ensureStableRuleIds();
    object["baseVisible"] = baseVisible;
    object["extVisible"] = extVisible;

    object["splitMode"] = splitMode;
    object["skipSlur"] = skipSlur;

    object["exportLanguage"] = exportLanguage;

    object["textEditFontSize"] = textEditFontSize;
    object["viewFontSize"] = viewFontSize;

    object["builtinSplitterEnabled"] = mapToJson(builtinSplitterEnabled);
    object["builtinTaggerEnabled"] = mapToJson(builtinTaggerEnabled);

    object["customSplitterRules"] = JsonUtils::serializeListToJson(customSplitterRules);
    object["customTaggerRules"] = JsonUtils::serializeListToJson(customTaggerRules);

    object["splitterOrder"] = JsonUtils::serializeStringList(splitterOrder);
    object["taggerOrder"] = JsonUtils::serializeStringList(taggerOrder);
}
