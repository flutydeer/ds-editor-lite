#include "tst_lyrics.h"

#include <QtTest/QTest>
#include "Model/AppOptions/Options/FillLyricOption.h"
#include "Modules/FillLyric/Utils/LyricRuleAutomationUtils.h"
#include "Modules/FillLyric/Utils/TaggerRuleOrder.h"
#include "Modules/FillLyric/Utils/TextSplitter.h"
#include "Modules/FillLyric/Utils/TextTagger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <filesystem>

namespace {

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

    bool writeSplitterRule(const QString &directory, const QString &name,
                           const QStringList &patterns) {
        QFile file(QDir(directory).filePath(name + QStringLiteral(".json")));
        const auto bytes =
            QJsonDocument(QJsonObject{
                              {QStringLiteral("regexes"), QJsonArray::fromStringList(patterns)}
        })
                .toJson();
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
               file.write(bytes) == bytes.size();
    }

    QStringList splitText(const QString &text) {
        QStringList result;
        for (const auto &part : FillLyric::TextSplitter::split(text.toUtf8().toStdString()))
            result.append(QString::fromUtf8(part.data(), static_cast<qsizetype>(part.size())));
        return result;
    }

    QStringList runtimeTags() {
        QStringList tags;
        for (const auto &result : FillLyric::TextTagger::tag({"same"}))
            tags.append(QString::fromStdString(result.tag));
        return tags;
    }

}

void LyricsTests::lyricRulesStableOrderPersistence() {
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

    QCOMPARE(reopened.customTaggerRules.size(), 1);
    QCOMPARE(reopened.customTaggerRules.front().name, QStringLiteral("custom-cmn"));
    QCOMPARE(reopened.customTaggerRules.front().language, QStringLiteral("cmn"));
    QCOMPARE(reopened.taggerOrder, customFirst);
    QCOMPARE(resolved, (QList<int>{1, 0}));
}

void LyricsTests::lyricRulesLegacyOrderMigration() {
    using namespace FillLyric;

    const QList<TaggerRuleIdentity> available{
        {.language = QStringLiteral("cmn"), .builtin = true },
        {.language = QStringLiteral("cmn"), .builtin = false},
    };
    const QStringList legacy{QStringLiteral("cmn"), QStringLiteral("cmn")};
    const auto resolved = TaggerRuleOrder::resolve(legacy, available);
    const auto migrated = TaggerRuleOrder::canonicalize(legacy, available);

    QCOMPARE(resolved, (QList<int>{0, 1}));
    QCOMPARE(migrated, (QStringList{
                           TaggerRuleOrder::key(available.at(0)),
                           TaggerRuleOrder::key(available.at(1)),
                       }));
}

void LyricsTests::lyricRulesStableAutomationRuleIdMigration() {
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
    QCOMPARE(migrated.customSplitterRules.size(), 1);
    QCOMPARE(migrated.customTaggerRules.size(), 1);
    const auto splitterId = migrated.customSplitterRules.front().ruleId;
    const auto taggerId = migrated.customTaggerRules.front().ruleId;

    FillLyricOption reopened;
    reopened.load(migrated.value());
    QCOMPARE(reopened.customSplitterRules.size(), 1);
    QCOMPARE(reopened.customTaggerRules.size(), 1);
    QVERIFY2(FillLyric::isAutomationRuleId(splitterId),
             "legacy custom rules should receive unique stable IDs");
    QVERIFY2(FillLyric::isAutomationRuleId(taggerId),
             "legacy custom rules should receive unique stable IDs");
    QVERIFY2(splitterId != taggerId, "legacy custom rules should receive unique stable IDs");
    QCOMPARE(reopened.customSplitterRules.front().ruleId, splitterId);
    QCOMPARE(reopened.customTaggerRules.front().ruleId, taggerId);
    QCOMPARE(migrated.customTaggerRules.front().name, QStringLiteral("legacy-language"));
    QCOMPARE(reopened.customTaggerRules.front().name, QStringLiteral("legacy-language"));
    const auto builtinId =
        FillLyric::builtinAutomationRuleId(QStringLiteral("splitter"), QStringLiteral("builtin"));
    QCOMPARE(builtinId, FillLyric::builtinAutomationRuleId(QStringLiteral("splitter"),
                                                           QStringLiteral("builtin")));
    QVERIFY2(builtinId != FillLyric::builtinAutomationRuleId(QStringLiteral("splitter"),
                                                             QStringLiteral("other")),
             "built-in rule IDs should be deterministic per kind and key");
}

void LyricsTests::lyricRulesRuntimeOrder() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto configDir = QDir(directory.path()).filePath(QStringLiteral("tagger"));
    QVERIFY(QDir().mkpath(configDir));
    QVERIFY(writeBuiltinRule(configDir));
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

    QVERIFY2(TextTagger::init(filesystemPath(configDir), filesystemPath(configDir)),
             "tagger test config should initialize");
    QVERIFY2(TextTagger::setCustomRules(customRules),
             "valid custom tagger rules should be installed");
    TextTagger::setRuleOrder({TaggerRuleOrder::key(custom), TaggerRuleOrder::key(builtin)});
    const auto customFirstInfo = TextTagger::ruleInfoList();
    QCOMPARE(customFirstInfo.size(), 2);
    QVERIFY2(!customFirstInfo.at(0).builtin,
             "runtime order should distinguish custom from builtin");
    QVERIFY2(customFirstInfo.at(1).builtin, "runtime order should distinguish custom from builtin");
    QCOMPARE(runtimeTags(), (QStringList{QStringLiteral("custom")}));

    TextTagger::setRuleOrder({TaggerRuleOrder::key(builtin), TaggerRuleOrder::key(custom)});
    QCOMPARE(runtimeTags(), (QStringList{QStringLiteral("builtin")}));

    QVERIFY2(TextTagger::init(filesystemPath(configDir), filesystemPath(configDir)),
             "tagger should reinitialize for legacy migration");
    QVERIFY2(TextTagger::setCustomRules(customRules),
             "custom tagger rules should reinstall after initialization");
    TextTagger::setRuleOrder({QStringLiteral("cmn"), QStringLiteral("cmn")});
    const auto legacyInfo = TextTagger::ruleInfoList();
    QCOMPARE(legacyInfo.size(), 2);
    QVERIFY2(legacyInfo.at(0).builtin,
             "legacy runtime order should retain both same-language rules");
    QVERIFY2(!legacyInfo.at(1).builtin,
             "legacy runtime order should retain both same-language rules");
    QCOMPARE(runtimeTags(), (QStringList{QStringLiteral("builtin")}));

    TextTagger::setRuleOrder({TaggerRuleOrder::key(custom), TaggerRuleOrder::key(builtin)});
    auto invalidRule = customCmnRule();
    invalidRule.entries = {
        {
         .type = QStringLiteral("dict"),
         .value = {QStringLiteral("missing-dictionary.txt")},
         .tag = QStringLiteral("invalid"),
         }
    };
    QVERIFY2(!TextTagger::setCustomRules({invalidRule}),
             "missing custom dictionaries must reject the whole update");
    const auto afterRejectedUpdate = TextTagger::ruleInfoList();
    QCOMPARE(afterRejectedUpdate.size(), 2);
    QVERIFY2(!afterRejectedUpdate.at(0).builtin,
             "rejected custom rules must preserve the previous runtime rules");
    QCOMPARE(afterRejectedUpdate.at(0).entries.size(), 1);
    QCOMPARE(afterRejectedUpdate.at(0).entries.front().tag, QStringLiteral("custom"));
    QCOMPARE(runtimeTags(), (QStringList{QStringLiteral("custom")}));
}

void LyricsTests::lyricRulesSplitterPreservesMixedLanguageText() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(writeSplitterRule(directory.path(), QStringLiteral("mixed"),
                              {QStringLiteral("([\\p{Han}])"), QStringLiteral("([A-Za-z]+)")}));
    QVERIFY(FillLyric::TextSplitter::init(filesystemPath(directory.path())));
    const auto input = QStringLiteral("你好 hello🙂カナ");
    const QStringList expected{QStringLiteral("你"), QStringLiteral("好"), QStringLiteral(" "),
                               QStringLiteral("hello"), QStringLiteral("🙂カナ")};
    const auto result = splitText(input);
    QCOMPARE(result, expected);
    QCOMPARE(result.join(QString{}), input);
}

void LyricsTests::lyricRulesSplitterEnabledRules_data() {
    QTest::addColumn<bool>("builtinEnabled");
    QTest::addColumn<bool>("customEnabled");
    QTest::addColumn<QStringList>("expected");
    QTest::newRow("both-enabled") << true << true
                                  << QStringList{QStringLiteral("中"), QStringLiteral("甲"),
                                                 QStringLiteral("a"), QStringLiteral("b")};
    QTest::newRow("builtin-only") << true << false
                                  << QStringList{QStringLiteral("中"), QStringLiteral("甲"),
                                                 QStringLiteral("ab")};
    QTest::newRow("custom-only") << false << true
                                 << QStringList{QStringLiteral("中甲"), QStringLiteral("a"),
                                                QStringLiteral("b")};
    QTest::newRow("both-disabled") << false << false << QStringList{QStringLiteral("中甲ab")};
}

void LyricsTests::lyricRulesSplitterEnabledRules() {
    QFETCH(bool, builtinEnabled);
    QFETCH(bool, customEnabled);
    QFETCH(QStringList, expected);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(writeSplitterRule(directory.path(), QStringLiteral("han"),
                              {QStringLiteral("([\\p{Han}])")}));
    QVERIFY(FillLyric::TextSplitter::init(filesystemPath(directory.path())));
    FillLyric::TextSplitter::setBuiltinEnabled({
        {QStringLiteral("han"), builtinEnabled}
    });
    CustomSplitterRule custom;
    custom.name = QStringLiteral("latin");
    custom.regexes = {QStringLiteral("(a)")};
    custom.enabled = customEnabled;
    FillLyric::TextSplitter::setCustomRules({custom});
    const auto input = QStringLiteral("中甲ab");
    const auto result = splitText(input);
    QCOMPARE(result, expected);
    QCOMPARE(result.join(QString{}), input);
}

void LyricsTests::lyricRulesSplitterRulePriority_data() {
    QTest::addColumn<QStringList>("order");
    QTest::addColumn<QStringList>("expected");
    QTest::newRow("builtin-first") << QStringList{QStringLiteral("pair"), QStringLiteral("tail")}
                                   << QStringList{QStringLiteral("ab"), QStringLiteral("c")};
    QTest::newRow("custom-first") << QStringList{QStringLiteral("tail"), QStringLiteral("pair")}
                                  << QStringList{QStringLiteral("a"), QStringLiteral("bc")};
}

void LyricsTests::lyricRulesSplitterRulePriority() {
    QFETCH(QStringList, order);
    QFETCH(QStringList, expected);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(writeSplitterRule(directory.path(), QStringLiteral("pair"), {QStringLiteral("(ab)")}));
    QVERIFY(FillLyric::TextSplitter::init(filesystemPath(directory.path())));
    CustomSplitterRule custom;
    custom.name = QStringLiteral("tail");
    custom.regexes = {QStringLiteral("(bc)")};
    FillLyric::TextSplitter::setCustomRules({custom});
    FillLyric::TextSplitter::setRuleOrder(order);
    const auto input = QStringLiteral("abc");
    const auto result = splitText(input);
    QCOMPARE(result, expected);
    QCOMPARE(result.join(QString{}), input);
}

void LyricsTests::lyricRulesSplitterEmptyMatchDoesNotDuplicateText() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(writeSplitterRule(directory.path(), QStringLiteral("empty-after-prefix"),
                              {QStringLiteral("(a*)")}));
    QVERIFY(FillLyric::TextSplitter::init(filesystemPath(directory.path())));
    const auto input = QStringLiteral("a中");
    const auto result = splitText(input);
    QCOMPARE(result, QStringList{input});
    QCOMPARE(result.join(QString{}), input);
}
