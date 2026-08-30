#include "Model/AppOptions/Options/FillLyricOption.h"
#include "Modules/FillLyric/Utils/TaggerRuleOrder.h"
#include "Modules/FillLyric/Utils/TextTagger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
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
                         reopened.customTaggerRules.front().language == QStringLiteral("cmn"),
                     QStringLiteral("custom rule should survive option reopen"));
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
        TextTagger::setCustomRules(customRules);
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
        TextTagger::setCustomRules(customRules);
        TextTagger::setRuleOrder({QStringLiteral("cmn"), QStringLiteral("cmn")});
        const auto legacyInfo = TextTagger::ruleInfoList();
        ok &=
            expect(legacyInfo.size() == 2 && legacyInfo.at(0).builtin && !legacyInfo.at(1).builtin,
                   QStringLiteral("legacy runtime order should retain both same-language rules"));
        ok &= expectTag(QStringLiteral("builtin"));
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
    ok &= testRuntimeOrder(configDir);

    return ok ? 0 : 1;
}
