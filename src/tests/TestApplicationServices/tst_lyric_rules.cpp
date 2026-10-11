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
    const auto originalSettings = harness.settings;
    const auto version = harness.core().documentVersion();
    const auto *undo = HistoryManager::instance()->nextUndoEntry();
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
    harness.settingsApplySucceeds = false;
    const auto failed = harness.core().settings().createLyricRule(applicationContext(), draft);
    harness.settingsApplySucceeds = true;
    QVERIFY(!failed);
    QCOMPARE(failed.getError().code, Automation::AutomationErrorCode::IoError);
    QCOMPARE(failed.getError().operationId, Automation::OperationIds::settings::update_fill_lyric);
    QCOMPARE(harness.settings, originalSettings);
    QCOMPARE(harness.lyricWrites, 0);
    QCOMPARE(harness.core().documentVersion(), version);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), undo);
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
    harness.lyricRulesSnapshot.prepend(created.get().rule);
    const auto persisted = harness.settings;
    const auto verifyDuplicate = [&](const Automation::LyricRuleDraftDto &duplicate,
                                     const QString &field) {
        const auto result =
            harness.core().settings().createLyricRule(applicationContext(), duplicate);
        QVERIFY(!result);
        QCOMPARE(result.getError().code, Automation::AutomationErrorCode::InvalidArgument);
        QCOMPARE(result.getError().fieldPath, field);
        QCOMPARE(harness.settings, persisted);
        QCOMPARE(harness.lyricWrites, 1);
        QCOMPARE(harness.core().documentVersion(), version);
        QCOMPARE(HistoryManager::instance()->nextUndoEntry(), undo);
    };
    verifyDuplicate(draft, QStringLiteral("name"));
    draft.name = QStringLiteral("second");
    if (tagger) {
        verifyDuplicate(draft, QStringLiteral("language"));
        draft.language = QStringLiteral("cmn");
    }
    const auto recovered = harness.core().settings().createLyricRule(applicationContext(), draft);
    QVERIFY(recovered && recovered.get().changed);
    QVERIFY(recovered.get().rule.ruleId != created.get().rule.ruleId);
    QCOMPARE(harness.lyricWrites, 2);
    QCOMPARE(harness.core().documentVersion(), version);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), undo);
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
    Automation::LyricRulePatchDto patch{.name = QStringLiteral("renamed")};
    if (tagger) {
        patch.language = QStringLiteral("cmn");
        patch.entries = QList<Automation::TaggerEntryDto>{
            {.type = QStringLiteral("array"),
             .value = {QStringLiteral("月")},
             .tag = QStringLiteral("word")}
        };
    }
    const auto renamed =
        harness.core().settings().updateLyricRule(applicationContext(), rule.ruleId, patch);
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
    if (tagger) {
        const auto &stored = harness.settings.fillLyric.customTaggerRules.first();
        QCOMPARE(stored.ruleId, rule.ruleId);
        QCOMPARE(stored.language, *patch.language);
        QVERIFY(stored.entries == *patch.entries);
        QCOMPARE(harness.settings.fillLyric.taggerOrder, QStringList{QStringLiteral("custom:cmn")});
    }
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

    Automation::LyricRuleDto builtinTagger{
        .ruleId = QStringLiteral("builtin-tagger-eng"),
        .kind = Automation::LyricRuleKind::Tagger,
        .builtin = true,
        .name = QStringLiteral("builtin"),
        .language = QStringLiteral("eng"),
        .enabled = true,
    };
    harness.lyricRulesSnapshot.append(builtinTagger);
    const auto disabledTagger =
        settings.setLyricRuleEnabled(applicationContext(), builtinTagger.ruleId, false);
    QVERIFY(disabledTagger && disabledTagger.get().changed);
    QCOMPARE(disabledTagger.get().rule.ruleId, builtinTagger.ruleId);
    QVERIFY(!disabledTagger.get().rule.enabled);
    QVERIFY(!harness.settings.fillLyric.builtinTaggerEnabled.value(QStringLiteral("eng"), true));
    harness.lyricRulesSnapshot.last() = disabledTagger.get().rule;
    const auto repeated =
        settings.setLyricRuleEnabled(applicationContext(), builtinTagger.ruleId, false);
    QVERIFY(repeated && !repeated.get().changed);
    QCOMPARE(harness.lyricWrites, 3);
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
    harness.lyricRulesSnapshot.removeIf(
        [&](const auto &entry) { return entry.ruleId == rule.ruleId; });
    const auto persisted = harness.settings;
    const auto version = harness.core().documentVersion();
    const auto *undo = HistoryManager::instance()->nextUndoEntry();
    const auto attempts = harness.settingsWriteAttempts;
    auto &settings = harness.core().settings();
    const auto verifyDeleted = [](const auto &result) {
        QVERIFY(!result);
        QCOMPARE(result.getError().code, Automation::AutomationErrorCode::NotFound);
        QCOMPARE(result.getError().fieldPath, QStringLiteral("rule_id"));
    };
    verifyDeleted(settings.updateLyricRule(applicationContext(), rule.ruleId,
                                           {.name = QStringLiteral("renamed")}));
    verifyDeleted(settings.setLyricRuleEnabled(applicationContext(), rule.ruleId, false));
    verifyDeleted(settings.moveLyricRule(applicationContext(), rule.ruleId, 0));
    verifyDeleted(settings.deleteLyricRule(applicationContext(), rule.ruleId));
    QCOMPARE(harness.settings, persisted);
    QCOMPARE(harness.settingsWriteAttempts, attempts);
    QCOMPARE(harness.lyricWrites, 1);
    QCOMPARE(harness.core().documentVersion(), version);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), undo);
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
