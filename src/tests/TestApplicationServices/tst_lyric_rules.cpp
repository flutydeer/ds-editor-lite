#include "tst_application_services.h"
#include "ApplicationHarness.h"

#include <QtTest>
#include <limits>

using namespace ApplicationTest;

namespace {
    Automation::LyricRuleDto builtinSplitter() {
        return {
            .ruleId = QStringLiteral("builtin-splitter-000000000000000000000000"),
            .kind = Automation::LyricRuleKind::Splitter,
            .builtin = true,
            .name = QStringLiteral("builtin"),
            .regexes = {QStringLiteral("(\\s+)")},
            .enabled = true,
            .order = 0,
            .engineOrderKey = QStringLiteral("builtin"),
        };
    }

    Automation::LyricRuleDto seedCustomRule(ApplicationHarness &harness, bool tagger) {
        Automation::LyricRuleDto rule{
            .ruleId = QStringLiteral("11111111-1111-4111-8111-111111111111"),
            .kind =
                tagger ? Automation::LyricRuleKind::Tagger : Automation::LyricRuleKind::Splitter,
            .builtin = false,
            .name = QStringLiteral("custom"),
            .language = tagger ? QStringLiteral("eng") : QString(),
            .regexes = tagger ? QStringList() : QStringList{QStringLiteral("([,，])")},
            .entries = tagger
                           ? QList<Automation::TaggerEntryDto>{{.type = QStringLiteral("array"),
                                                                .value = {QStringLiteral("star")},
                                                                .tag = QStringLiteral("word")}}
                           : QList<Automation::TaggerEntryDto>(),
            .enabled = true,
            .order = tagger ? 0 : 1,
            .engineOrderKey = tagger ? QStringLiteral("custom:eng") : QStringLiteral("custom"),
        };
        harness.lyricRulesSnapshot = {builtinSplitter(), rule};
        if (tagger) {
            harness.settings.fillLyric.customTaggerRules = {
                {.ruleId = rule.ruleId,
                 .name = rule.name,
                 .language = rule.language,
                 .entries = rule.entries,
                 .enabled = true}
            };
            harness.settings.fillLyric.taggerOrder = {rule.engineOrderKey};
        } else {
            harness.settings.fillLyric.customSplitterRules = {
                {.ruleId = rule.ruleId,
                 .name = rule.name,
                 .regexes = rule.regexes,
                 .enabled = true,
                 .order = 1}
            };
            harness.settings.fillLyric.splitterOrder = {QStringLiteral("builtin"),
                                                        rule.engineOrderKey};
        }
        return rule;
    }
}

void ApplicationServicesTests::lyricRuleSnapshotAndPreview() {
    ApplicationHarness harness;
    harness.lyricRulesSnapshot = {builtinSplitter()};
    auto &settings = harness.core().settings();
    const auto initial = settings.listLyricRules();
    QVERIFY(initial);
    QVERIFY(initial.get() == harness.lyricRulesSnapshot);

    const auto preview = settings.createLyricRule(applicationContext(true),
                                                  {.kind = Automation::LyricRuleKind::Splitter,
                                                   .name = QStringLiteral("punctuation"),
                                                   .regexes = {QStringLiteral("([,，])")},
                                                   .enabled = true,
                                                   .position = 0});
    QVERIFY(preview);
    QVERIFY(preview.get().validatedOnly);
    QCOMPARE(preview.get().rule.order, 0);
    QCOMPARE(harness.lyricWrites, 0);
    QVERIFY(harness.settings.fillLyric.customSplitterRules.isEmpty());
    harness.lyricRulesSnapshot.clear();
    QCOMPARE(initial.get().size(), 1);
    QVERIFY(initial.get().first().builtin);
}

void ApplicationServicesTests::lyricRuleCreation_data() {
    QTest::addColumn<bool>("tagger");
    QTest::newRow("splitter") << false;
    QTest::newRow("tagger") << true;
}

void ApplicationServicesTests::lyricRuleCreation() {
    QFETCH(bool, tagger);
    ApplicationHarness harness;
    harness.lyricRulesSnapshot = {builtinSplitter()};
    Automation::LyricRuleDraftDto draft{
        .kind = tagger ? Automation::LyricRuleKind::Tagger : Automation::LyricRuleKind::Splitter,
        .name = QStringLiteral("custom"),
        .language = tagger ? QStringLiteral("eng") : QString(),
        .regexes = tagger ? QStringList() : QStringList{QStringLiteral("([,，])")},
        .entries = tagger ? QList<Automation::TaggerEntryDto>{{.type = QStringLiteral("array"),
                                                               .value = {QStringLiteral("star")},
                                                               .tag = QStringLiteral("word")}}
                          : QList<Automation::TaggerEntryDto>(),
        .position = 0,
    };
    const auto created = harness.core().settings().createLyricRule(applicationContext(), draft);
    QVERIFY(created);
    QVERIFY(created.get().changed);
    QVERIFY(!created.get().rule.ruleId.isEmpty());
    QCOMPARE(created.get().rule.name, draft.name);
    QCOMPARE(created.get().rule.order, 0);
    QCOMPARE(harness.lyricWrites, 1);
    if (tagger) {
        const auto &stored = harness.settings.fillLyric.customTaggerRules;
        QCOMPARE(stored.size(), 1);
        QCOMPARE(stored.first().ruleId, created.get().rule.ruleId);
        QCOMPARE(stored.first().name, draft.name);
        QCOMPARE(stored.first().language, draft.language);
        QVERIFY(stored.first().entries == draft.entries);
    } else {
        const auto &stored = harness.settings.fillLyric.customSplitterRules;
        QCOMPARE(stored.size(), 1);
        QCOMPARE(stored.first().ruleId, created.get().rule.ruleId);
        QCOMPARE(stored.first().regexes, draft.regexes);
        QCOMPARE(harness.settings.fillLyric.splitterOrder,
                 (QStringList{QStringLiteral("custom"), QStringLiteral("builtin")}));
    }
}

void ApplicationServicesTests::lyricRuleRename_data() {
    QTest::addColumn<bool>("tagger");
    QTest::newRow("splitter") << false;
    QTest::newRow("tagger") << true;
}

void ApplicationServicesTests::lyricRuleRename() {
    QFETCH(bool, tagger);
    ApplicationHarness harness;
    const auto rule = seedCustomRule(harness, tagger);
    const auto renamed = harness.core().settings().updateLyricRule(
        applicationContext(), rule.ruleId, {.name = QStringLiteral("renamed")});
    QVERIFY2(renamed,
             qPrintable(renamed ? QString()
                                : QStringLiteral("%1: %2").arg(renamed.getError().fieldPath,
                                                               renamed.getError().message)));
    QCOMPARE(renamed.get().rule.ruleId, rule.ruleId);
    QCOMPARE(renamed.get().rule.name, QStringLiteral("renamed"));
    QCOMPARE(harness.lyricWrites, 1);
    QCOMPARE(tagger ? harness.settings.fillLyric.customTaggerRules.first().name
                    : harness.settings.fillLyric.customSplitterRules.first().name,
             QStringLiteral("renamed"));
}

void ApplicationServicesTests::lyricRuleEnableAndOrder() {
    ApplicationHarness harness;
    const auto rule = seedCustomRule(harness, false);
    auto &settings = harness.core().settings();
    const auto disabled = settings.setLyricRuleEnabled(applicationContext(), rule.ruleId, false);
    QVERIFY2(disabled,
             qPrintable(disabled ? QString()
                                 : QStringLiteral("%1: %2").arg(disabled.getError().fieldPath,
                                                                disabled.getError().message)));
    QVERIFY(!disabled.get().rule.enabled);
    QVERIFY(!harness.settings.fillLyric.customSplitterRules.first().enabled);
    harness.lyricRulesSnapshot[1] = disabled.get().rule;

    const auto moved = settings.moveLyricRule(applicationContext(), rule.ruleId, 0);
    QVERIFY2(moved, qPrintable(moved ? QString()
                                     : QStringLiteral("%1: %2").arg(moved.getError().fieldPath,
                                                                    moved.getError().message)));
    QCOMPARE(moved.get().rule.order, 0);
    QCOMPARE(harness.settings.fillLyric.splitterOrder,
             (QStringList{QStringLiteral("custom"), QStringLiteral("builtin")}));
    QCOMPARE(harness.lyricWrites, 2);
}

void ApplicationServicesTests::invalidLyricRuleEdits_data() {
    QTest::addColumn<int>("scenario");
    QTest::newRow("splitter-rejects-language") << 0;
    QTest::newRow("splitter-rejects-invalid-regex") << 1;
    QTest::newRow("tagger-rejects-regex") << 2;
    QTest::newRow("builtin-content-is-immutable") << 3;
}

void ApplicationServicesTests::invalidLyricRuleEdits() {
    QFETCH(int, scenario);
    ApplicationHarness harness;
    const auto rule = seedCustomRule(harness, scenario == 2);
    const auto before = harness.settings.fillLyric;
    Automation::LyricRulePatchDto patch;
    if (scenario == 0)
        patch.language = QStringLiteral("cmn");
    else if (scenario == 1)
        patch.regexes = QStringList{QStringLiteral("(")};
    else if (scenario == 2)
        patch.regexes = QStringList{QStringLiteral("([a-z]+)")};
    else
        patch.name = QStringLiteral("forbidden");
    const auto updated = harness.core().settings().updateLyricRule(
        applicationContext(), scenario == 3 ? builtinSplitter().ruleId : rule.ruleId, patch);
    QVERIFY(!updated);
    QCOMPARE(updated.getError().code, Automation::AutomationErrorCode::InvalidArgument);
    QVERIFY(harness.settings.fillLyric == before);
    QCOMPARE(harness.lyricWrites, 0);
}

void ApplicationServicesTests::lyricRuleDeletion_data() {
    QTest::addColumn<bool>("tagger");
    QTest::newRow("splitter") << false;
    QTest::newRow("tagger") << true;
}

void ApplicationServicesTests::lyricRuleDeletion() {
    QFETCH(bool, tagger);
    ApplicationHarness harness;
    const auto rule = seedCustomRule(harness, tagger);
    const auto deleted =
        harness.core().settings().deleteLyricRule(applicationContext(), rule.ruleId);
    QVERIFY(deleted);
    QCOMPARE(deleted.get().ruleId, rule.ruleId);
    QVERIFY(harness.settings.fillLyric.customSplitterRules.isEmpty());
    QVERIFY(harness.settings.fillLyric.customTaggerRules.isEmpty());
    QCOMPARE(harness.lyricWrites, 1);
}

void ApplicationServicesTests::lyricRuleHostValidationFailure() {
    ApplicationHarness harness;
    const auto before = harness.settings.fillLyric;
    harness.lyricValidationError = Automation::AutomationError::invalidArgument(
        QStringLiteral("entries.value"), QStringLiteral("Tagger dictionary was not found"));
    const auto created = harness.core().settings().createLyricRule(
        applicationContext(), {
                                  .kind = Automation::LyricRuleKind::Tagger,
                                  .name = QStringLiteral("missing-dict"),
                                  .language = QStringLiteral("und-x-missing"),
                                  .entries = {{.type = QStringLiteral("dict"),
                                               .value = {QStringLiteral("missing-dictionary.txt")},
                                               .tag = QStringLiteral("word")}},
                              });
    QVERIFY(!created);
    QCOMPARE(created.getError().code, Automation::AutomationErrorCode::InvalidArgument);
    QCOMPARE(created.getError().fieldPath, QStringLiteral("entries.value"));
    QVERIFY(harness.settings.fillLyric == before);
    QCOMPARE(harness.lyricWrites, 0);
    QCOMPARE(harness.lyricValidationCalls, 1);
}

void ApplicationServicesTests::lyricRuleTestDelegatesWithoutMutation() {
    ApplicationHarness harness;
    const auto before = harness.settings;
    const auto tested = harness.core().settings().testLyricRules(QStringLiteral("一闪一闪"));
    QVERIFY(tested);
    QCOMPARE(harness.lyricTestCalls, 1);
    QCOMPARE(harness.lastLyricTestText, QStringLiteral("一闪一闪"));
    QCOMPARE(tested.get().splitTokens, QStringList{QStringLiteral("一闪一闪")});
    QVERIFY(harness.settings == before);
    QCOMPARE(harness.lyricWrites, 0);
}

void ApplicationServicesTests::invalidLyricRulesDoNotPersist() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    auto settings = harness.settings.fillLyric;
    settings.customSplitterRules.append(
        {.name = QStringLiteral("splitter"), .regexes = {QStringLiteral("[,，]")}});
    settings.customTaggerRules.append({.name = QStringLiteral("tagger"),
                                       .language = QStringLiteral("cmn"),
                                       .entries = {{.type = QStringLiteral("regex"),
                                                    .value = {QStringLiteral("^la$")},
                                                    .tag = QStringLiteral("tag")}}});
    QVERIFY(runtime.settings().updateFillLyric(applicationContext(), settings));
    const auto writesBefore = harness.settingsWrites;
    auto duplicateSplitter = harness.settings.fillLyric;
    duplicateSplitter.customSplitterRules.append(duplicateSplitter.customSplitterRules.first());
    auto duplicateTagger = harness.settings.fillLyric;
    duplicateTagger.customTaggerRules.append(duplicateTagger.customTaggerRules.first());
    auto invalidTagger = harness.settings.fillLyric;
    invalidTagger.customTaggerRules.first().entries.first().type = QStringLiteral("unsupported");
    const auto rejectedSplitter =
        runtime.settings().updateFillLyric(applicationContext(), duplicateSplitter);
    const auto rejectedTagger =
        runtime.settings().updateFillLyric(applicationContext(), duplicateTagger);
    const auto rejectedEntry =
        runtime.settings().updateFillLyric(applicationContext(), invalidTagger);
    QVERIFY2(
        (!rejectedSplitter &&
         rejectedSplitter.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
         rejectedSplitter.getError().operationId ==
             Automation::OperationIds::settings::update_fill_lyric),
        qPrintable(QStringLiteral("duplicate lyric splitter names must be rejected")));
    QVERIFY2((!rejectedTagger &&
              rejectedTagger.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
              rejectedTagger.getError().operationId ==
                  Automation::OperationIds::settings::update_fill_lyric),
             qPrintable(QStringLiteral("duplicate lyric tagger languages must be rejected")));
    QVERIFY2((!rejectedEntry &&
              rejectedEntry.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
              rejectedEntry.getError().operationId ==
                  Automation::OperationIds::settings::update_fill_lyric),
             qPrintable(QStringLiteral("unsupported lyric tagger entry type must be rejected")));
    QVERIFY2((harness.settingsWrites == writesBefore),
             qPrintable(QStringLiteral("invalid lyric rules must not reach persistence")));
}
