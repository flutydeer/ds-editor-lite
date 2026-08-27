#include "Model/AppOptions/Options/FillLyricOption.h"
#include "Modules/FillLyric/Utils/LyricRuleAutomationUtils.h"
#include "Modules/FillLyric/Utils/TaggerRuleOrder.h"
#include "Modules/FillLyric/Utils/TextTagger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>

#include <filesystem>

namespace {
    bool expect(const bool condition, const QString &message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    std::filesystem::path filesystemPath(const QString &path) {
#ifdef Q_OS_WIN
        return std::filesystem::path(path.toStdWString());
#else
        return std::filesystem::path(path.toStdString());
#endif
    }

    CustomTaggerRule customCmnRule() {
        CustomTaggerRule rule;
        rule.name = QStringLiteral("custom-cmn");
        rule.language = QStringLiteral("cmn");
        rule.entries.append({
            .type = QStringLiteral("array"),
            .value = {QStringLiteral("same")},
            .tag = QStringLiteral("custom"),
        });
        return rule;
    }

    bool writeBuiltinRule(const QString &configDir) {
        QFile file(QDir(configDir).filePath(QStringLiteral("cmn.json")));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        return file.write(R"({
            "language": "cmn",
            "tagger": [{
                "type": "array",
                "value": ["same"],
                "tag": "builtin"
            }]
        })") > 0;
    }

    bool expectTag(const QString &expected) {
        const auto result = FillLyric::TextTagger::tag({"same"});
        return expect(result.size() == 1 && QString::fromStdString(result.front().tag) == expected,
                      QStringLiteral("runtime winner should be %1").arg(expected));
    }

    bool testStableOrderPersistence() {
        using namespace FillLyric;

        const QList<TaggerRuleIdentity> available{
            {.language = QStringLiteral("cmn"), .builtin = true },
            {.language = QStringLiteral("cmn"), .builtin = false},
        };
        const QStringList customFirst{
            TaggerRuleOrder::key(available.at(1)),
            TaggerRuleOrder::key(available.at(0)),
        };

        FillLyricOption saved;
        saved.customTaggerRules.append(customCmnRule());
        saved.taggerOrder = customFirst;

        FillLyricOption reopened;
        reopened.load(saved.value());
        const auto resolved = TaggerRuleOrder::resolve(reopened.taggerOrder, available);

        bool ok = true;
        ok &= expect(reopened.customTaggerRules.size() == 1 &&
                         reopened.customTaggerRules.front().name == QStringLiteral("custom-cmn") &&
                         reopened.customTaggerRules.front().language == QStringLiteral("cmn"),
                     QStringLiteral("custom rule name and language should survive option reopen"));
        ok &= expect(reopened.taggerOrder == customFirst,
                     QStringLiteral("source-qualified order keys should survive option reopen"));
        ok &= expect(
            resolved == QList<int>{1, 0},
            QStringLiteral("reopened custom rule should remain before same-language builtin"));
        return ok;
    }

    bool testLegacyOrderMigration() {
        using namespace FillLyric;

        const QList<TaggerRuleIdentity> available{
            {.language = QStringLiteral("cmn"), .builtin = true },
            {.language = QStringLiteral("cmn"), .builtin = false},
        };
        const QStringList legacy{QStringLiteral("cmn"), QStringLiteral("cmn")};
        const auto resolved = TaggerRuleOrder::resolve(legacy, available);
        const auto migrated = TaggerRuleOrder::canonicalize(legacy, available);

        bool ok = true;
        ok &= expect(
            resolved == QList<int>{0, 1},
            QStringLiteral("legacy duplicate language entries should retain both rule sources"));
        ok &= expect(migrated ==
                         QStringList{
                             TaggerRuleOrder::key(available.at(0)),
                             TaggerRuleOrder::key(available.at(1)),
                         },
                     QStringLiteral("legacy order should canonicalize to source-qualified keys"));
        return ok;
    }

    bool testStableAutomationRuleIdMigration() {
        const QJsonObject legacy{
            {QStringLiteral("customSplitterRules"),
             QJsonArray{QJsonObject{
                 {QStringLiteral("name"), QStringLiteral("legacy-splitter")},
                 {QStringLiteral("regexes"), QJsonArray{QStringLiteral("([,])")}},
             }}},
            {QStringLiteral("customTaggerRules"),
             QJsonArray{QJsonObject{
                 {QStringLiteral("language"), QStringLiteral("legacy-language")},
                 {QStringLiteral("tagger"),
                  QJsonArray{QJsonObject{
                      {QStringLiteral("type"), QStringLiteral("array")},
                      {QStringLiteral("value"), QJsonArray{QStringLiteral("la")}},
                      {QStringLiteral("tag"), QStringLiteral("word")},
                  }}},
             }}},
        };
        FillLyricOption migrated;
        migrated.load(legacy);
        const auto splitterId = migrated.customSplitterRules.front().ruleId;
        const auto taggerId = migrated.customTaggerRules.front().ruleId;

        FillLyricOption reopened;
        reopened.load(migrated.value());
        bool ok = true;
        ok &= expect(FillLyric::isAutomationRuleId(splitterId) &&
                         FillLyric::isAutomationRuleId(taggerId) && splitterId != taggerId,
                     QStringLiteral("legacy custom rules should receive unique stable IDs"));
        ok &= expect(reopened.customSplitterRules.front().ruleId == splitterId &&
                         reopened.customTaggerRules.front().ruleId == taggerId,
                     QStringLiteral("migrated custom rule IDs should survive persistence"));
        ok &=
            expect(migrated.customTaggerRules.front().name == QStringLiteral("legacy-language") &&
                       reopened.customTaggerRules.front().name == QStringLiteral("legacy-language"),
                   QStringLiteral("legacy tagger names should migrate and survive persistence"));
        const auto builtinId = FillLyric::builtinAutomationRuleId(QStringLiteral("splitter"),
                                                                  QStringLiteral("builtin"));
        ok &= expect(builtinId == FillLyric::builtinAutomationRuleId(QStringLiteral("splitter"),
                                                                     QStringLiteral("builtin")) &&
                         builtinId != FillLyric::builtinAutomationRuleId(QStringLiteral("splitter"),
                                                                         QStringLiteral("other")),
                     QStringLiteral("built-in rule IDs should be deterministic per kind and key"));
        return ok;
    }

    bool testRuntimeOrder(const QString &configDir) {
        using namespace FillLyric;

        const TaggerRuleIdentity builtin{
            .language = QStringLiteral("cmn"),
            .builtin = true,
        };
        const TaggerRuleIdentity custom{
            .language = QStringLiteral("cmn"),
            .builtin = false,
        };
        const QList<CustomTaggerRule> customRules{customCmnRule()};

        bool ok = true;
        ok &= expect(TextTagger::init(filesystemPath(configDir), filesystemPath(configDir)),
                     QStringLiteral("tagger test config should initialize"));
        ok &= expect(TextTagger::setCustomRules(customRules),
                     QStringLiteral("valid custom tagger rules should be installed"));
        TextTagger::setRuleOrder({TaggerRuleOrder::key(custom), TaggerRuleOrder::key(builtin)});
        const auto customFirstInfo = TextTagger::ruleInfoList();
        ok &= expect(customFirstInfo.size() == 2 && !customFirstInfo.at(0).builtin &&
                         customFirstInfo.at(1).builtin,
                     QStringLiteral("runtime order should distinguish custom from builtin"));
        ok &= expectTag(QStringLiteral("custom"));

        TextTagger::setRuleOrder({TaggerRuleOrder::key(builtin), TaggerRuleOrder::key(custom)});
        ok &= expectTag(QStringLiteral("builtin"));

        ok &= expect(TextTagger::init(filesystemPath(configDir), filesystemPath(configDir)),
                     QStringLiteral("tagger should reinitialize for legacy migration"));
        ok &= expect(TextTagger::setCustomRules(customRules),
                     QStringLiteral("custom tagger rules should reinstall after initialization"));
        TextTagger::setRuleOrder({QStringLiteral("cmn"), QStringLiteral("cmn")});
        const auto legacyInfo = TextTagger::ruleInfoList();
        ok &=
            expect(legacyInfo.size() == 2 && legacyInfo.at(0).builtin && !legacyInfo.at(1).builtin,
                   QStringLiteral("legacy runtime order should retain both same-language rules"));
        ok &= expectTag(QStringLiteral("builtin"));

        TextTagger::setRuleOrder({TaggerRuleOrder::key(custom), TaggerRuleOrder::key(builtin)});
        auto invalidRule = customCmnRule();
        invalidRule.entries = {
            {
             .type = QStringLiteral("dict"),
             .value = {QStringLiteral("missing-dictionary.txt")},
             .tag = QStringLiteral("invalid"),
             }
        };
        ok &= expect(!TextTagger::setCustomRules({invalidRule}),
                     QStringLiteral("missing custom dictionaries must reject the whole update"));
        const auto afterRejectedUpdate = TextTagger::ruleInfoList();
        ok &= expect(
            afterRejectedUpdate.size() == 2 && !afterRejectedUpdate.at(0).builtin &&
                afterRejectedUpdate.at(0).entries.front().tag == QStringLiteral("custom"),
            QStringLiteral("rejected custom rules must preserve the previous runtime rules"));
        ok &= expectTag(QStringLiteral("custom"));
        return ok;
    }
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    QTemporaryDir tempDir;
    bool ok = expect(tempDir.isValid(), QStringLiteral("temporary directory should be available"));
    const auto configDir = QDir(tempDir.path()).filePath(QStringLiteral("tagger"));
    ok &= expect(QDir().mkpath(configDir),
                 QStringLiteral("tagger config directory should be created"));
    ok &= expect(writeBuiltinRule(configDir),
                 QStringLiteral("builtin tagger fixture should be written"));
    ok &= testStableOrderPersistence();
    ok &= testLegacyOrderMigration();
    ok &= testStableAutomationRuleIdMigration();
    ok &= testRuntimeOrder(configDir);

    return ok ? 0 : 1;
}
