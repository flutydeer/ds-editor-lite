#ifndef FILLLYRICOPTION_H
#define FILLLYRICOPTION_H

#include "Model/AppOptions/IOption.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

struct CustomSplitterRule {
    QString name;
    QStringList regexes;
    bool enabled = true;
    int order = 0;

    QJsonObject toJson() const;
    static CustomSplitterRule fromJson(const QJsonObject &obj);
};

struct CustomTaggerEntry {
    QString type;       // "regex" or "array"
    QStringList value;
    QString tag;
    bool discard = false;

    QJsonObject toJson() const;
    static CustomTaggerEntry fromJson(const QJsonObject &obj);
};

struct CustomTaggerRule {
    QString language;
    QList<CustomTaggerEntry> entries;
    bool enabled = true;

    QJsonObject toJson() const;
    static CustomTaggerRule fromJson(const QJsonObject &obj);
};

class FillLyricOption final : public IOption {
public:
    explicit FillLyricOption() : IOption("fillLyric") {
    }

    void load(const QJsonObject &object) override;

    bool baseVisible = true;
    bool extVisible = false;

    int splitMode = 0;
    bool skipSlur = false;

    bool exportLanguage = false;

    double textEditFontSize = 11;
    double viewFontSize = 12;

    // Built-in rule enable states (key=config name, value=enabled)
    // Missing keys are treated as enabled
    QMap<QString, bool> builtinSplitterEnabled;
    QMap<QString, bool> builtinTaggerEnabled;

    // User custom rules
    QList<CustomSplitterRule> customSplitterRules;
    QList<CustomTaggerRule> customTaggerRules;

    // Execution order (list of rule names/languages, includes both builtin and custom)
    // Empty means default order
    QStringList splitterOrder;
    QStringList taggerOrder;

protected:
    void save(QJsonObject &object) override;
};


#endif // FILLLYRICOPTION_H
