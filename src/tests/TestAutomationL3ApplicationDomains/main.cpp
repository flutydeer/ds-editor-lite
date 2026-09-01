#include "TestRuntime.h"

#include "Automation/OperationIds.h"
#include "Modules/FillLyric/Utils/TaggerRuleOrder.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSet>

#include <algorithm>

namespace {
    int failures = 0;

    void check(const bool condition, const QString &message) {
        if (condition)
            return;
        qCritical().noquote() << message;
        ++failures;
    }

    Automation::ApplicationCommandContext applicationContext(const bool validateOnly = false) {
        return {
            .validateOnly = validateOnly,
            .source = Automation::InvocationSource::Test,
            .clientId = QStringLiteral("l3-application-domain-test"),
        };
    }

    Automation::SettingsSnapshotDto validSettings() {
        Automation::SettingsSnapshotDto settings;
        settings.general.uiLanguage = QStringLiteral("system");
        settings.general.defaultSingingLanguage = QStringLiteral("cmn");
        settings.general.defaultLyrics.insert(QStringLiteral("cmn"), QStringLiteral("啦"));
        settings.general.packageSearchPaths = {QStringLiteral("C:/allowed/voices"),
                                               QStringLiteral("D:/private/voices")};
        settings.appearance.themeId = QStringLiteral("system");
        settings.inference.executionProvider = QStringLiteral("CPU");
        settings.inference.cacheDirectory = QStringLiteral("cache");
        for (int index = 0; index < 4; ++index) {
            settings.audio.pseudoSingerSynthesizers.append({
                .generator = index,
                .amplitude = -12.0,
                .attackMilliseconds = 10,
                .decayMilliseconds = 1000,
                .decayRatio = 0.5,
                .releaseMilliseconds = 50,
            });
        }
        return settings;
    }

    QList<Automation::LyricRuleDto> lyricRules(const Automation::FillLyricSettingsDto &settings) {
        QList<Automation::LyricRuleDto> rules{
            {
             .ruleId = QStringLiteral("builtin-splitter-000000000000000000000000"),
             .kind = Automation::LyricRuleKind::Splitter,
             .builtin = true,
             .name = QStringLiteral("builtin"),
             .regexes = {QStringLiteral("(\\s+)")},
             .enabled = settings.builtinSplitterEnabled.value(QStringLiteral("builtin"), true),
             .engineOrderKey = QStringLiteral("builtin"),
             }
        };
        for (const auto &custom : settings.customSplitterRules) {
            rules.append({
                .ruleId = custom.ruleId,
                .kind = Automation::LyricRuleKind::Splitter,
                .builtin = false,
                .name = custom.name,
                .regexes = custom.regexes,
                .enabled = custom.enabled,
                .order = custom.order,
                .engineOrderKey = custom.name,
            });
        }
        QList<Automation::LyricRuleDto> ordered;
        QSet<QString> consumed;
        for (const auto &key : settings.splitterOrder) {
            const auto found = std::find_if(rules.cbegin(), rules.cend(), [&](const auto &rule) {
                return rule.engineOrderKey == key && !consumed.contains(rule.ruleId);
            });
            if (found != rules.cend()) {
                ordered.append(*found);
                consumed.insert(found->ruleId);
            }
        }
        for (const auto &rule : std::as_const(rules)) {
            if (!consumed.contains(rule.ruleId))
                ordered.append(rule);
        }
        for (qsizetype index = 0; index < ordered.size(); ++index)
            ordered[index].order = static_cast<int>(index);

        QList<Automation::LyricRuleDto> taggers;
        for (const auto &custom : settings.customTaggerRules) {
            taggers.append({
                .ruleId = custom.ruleId,
                .kind = Automation::LyricRuleKind::Tagger,
                .builtin = false,
                .name = custom.name,
                .language = custom.language,
                .entries = custom.entries,
                .enabled = custom.enabled,
                .engineOrderKey = FillLyric::TaggerRuleOrder::key(
                    {.language = custom.language, .builtin = false}),
            });
        }
        QList<Automation::LyricRuleDto> orderedTaggers;
        QSet<QString> consumedTaggers;
        for (const auto &key : settings.taggerOrder) {
            const auto found =
                std::find_if(taggers.cbegin(), taggers.cend(), [&](const auto &rule) {
                    return rule.engineOrderKey == key && !consumedTaggers.contains(rule.ruleId);
                });
            if (found != taggers.cend()) {
                orderedTaggers.append(*found);
                consumedTaggers.insert(found->ruleId);
            }
        }
        for (const auto &rule : std::as_const(taggers)) {
            if (!consumedTaggers.contains(rule.ruleId))
                orderedTaggers.append(rule);
        }
        for (qsizetype index = 0; index < orderedTaggers.size(); ++index)
            orderedTaggers[index].order = static_cast<int>(index);
        ordered.append(orderedTaggers);
        return ordered;
    }

    struct Harness {
        Automation::SettingsSnapshotDto settings = validSettings();
        QList<Automation::PackageDto> packages{
            {
             .id = QStringLiteral("voice.package"),
             .version = QVersionNumber(1, 0),
             .path = QStringLiteral("D:/private/voice-v1"),
             },
            {
             .id = QStringLiteral("voice.package"),
             .version = QVersionNumber(2,                                         0),
             .path = QStringLiteral("C:/allowed/voice-v2"),
             },
        };
        int settingsWrites = 0;
        int lyricWrites = 0;
        int refreshStarts = 0;
        bool settingsApplySucceeds = true;

        template <typename T, typename Member>
        std::function<bool(const T &)> apply(Member member) {
            return [this, member](const T &value) {
                if (!settingsApplySucceeds)
                    return false;
                settings.*member = value;
                ++settingsWrites;
                return true;
            };
        }

        Automation::SettingsRuntimeServices settingsServices() {
            Automation::SettingsRuntimeServices services;
            services.snapshot = [this] { return settings; };
            services.applyGeneral =
                apply<Automation::GeneralSettingsDto>(&Automation::SettingsSnapshotDto::general);
            services.applyAppearance = apply<Automation::AppearanceSettingsDto>(
                &Automation::SettingsSnapshotDto::appearance);
            services.applyInference = apply<Automation::InferenceSettingsDto>(
                &Automation::SettingsSnapshotDto::inference);
            services.applyAudio =
                apply<Automation::AudioSettingsDto>(&Automation::SettingsSnapshotDto::audio);
            services.applyFillLyric = [this](const Automation::FillLyricSettingsDto &value) {
                if (!settingsApplySucceeds)
                    return false;
                settings.fillLyric = value;
                ++settingsWrites;
                ++lyricWrites;
                return true;
            };
            services.validateFillLyricRuntime = [](const Automation::FillLyricSettingsDto &value) {
                for (const auto &rule : value.customTaggerRules) {
                    for (const auto &entry : rule.entries) {
                        if (entry.type == QStringLiteral("dict") &&
                            entry.value.contains(QStringLiteral("missing-dictionary.txt"))) {
                            return Automation::AutomationResult<Automation::AutomationUnit>(
                                Automation::AutomationError::invalidArgument(
                                    QStringLiteral("entries.value"),
                                    QStringLiteral("Tagger dictionary was not found")));
                        }
                    }
                }
                return Automation::AutomationResult<Automation::AutomationUnit>(
                    Automation::AutomationUnit{});
            };
            services.lyricRules = [this] { return ::lyricRules(settings.fillLyric); };
            services.testLyricRules = [](const QString &text) {
                return Automation::AutomationResult<Automation::LyricRuleTestResultDto>({
                    .splitTokens = {text},
                    .taggedTokens = {{.lyric = text,
                                      .language = QStringLiteral("unknown"),
                                      .tag = QStringLiteral("unknown")}},
                });
            };
            return services;
        }

        Automation::PackageRuntimeServices packageServices() {
            Automation::PackageRuntimeServices services;
            services.installedPackages = [this] { return packages; };
            services.refreshPackages = [this](Automation::PackageRefreshCommitGate commitGate,
                                              Automation::PackageRefreshCompletion completion) {
                ++refreshStarts;
                if (commitGate && !commitGate())
                    return Automation::AutomationResult<Automation::AutomationUnit>(
                        Automation::AutomationUnit{});
                completion(Automation::PackageRefreshResultDto{
                    .packages = static_cast<int>(packages.size()),
                    .added = {QStringLiteral("new.package@1.0")},
                    .failures = {{.path = QStringLiteral("D:/private/broken"),
                                  .reason = QStringLiteral("broken")}},
                });
                return Automation::AutomationResult<Automation::AutomationUnit>(
                    Automation::AutomationUnit{});
            };
            return services;
        }
    };

    void testPublicSettings(Automation::CoreRuntime &runtime, Harness &harness) {
        const auto version = runtime.documentVersion();
        const auto projected = runtime.settings().queryPublicSettings(
            {QStringLiteral("package_search_paths")},
            [](const QString &path) -> std::optional<QString> {
                if (path.startsWith(QStringLiteral("C:/allowed")))
                    return path;
                return std::nullopt;
            });
        check(projected && projected.get().packageSearchPaths &&
                  projected.get().packageSearchPaths->configured ==
                      QStringList{QStringLiteral("C:/allowed/voices")},
              QStringLiteral("settings.query must filter package paths through the projection"));
        const auto unknown =
            runtime.settings().queryPublicSettings({QStringLiteral("not_a_public_domain")});
        check(!unknown &&
                  unknown.getError().code == Automation::AutomationErrorCode::InvalidArgument,
              QStringLiteral("settings.query must reject domains outside the explicit allowlist"));

        const auto writesBeforePreview = harness.settingsWrites;
        const auto preview = runtime.settings().updateUiLanguage(
            applicationContext(true), {.uiLanguage = QStringLiteral("en_US")});
        check(preview && preview.get().validatedOnly && preview.get().changed &&
                  harness.settingsWrites == writesBeforePreview &&
                  harness.settings.general.uiLanguage == QStringLiteral("system"),
              QStringLiteral("settings validate_only must not persist or change effective state"));

        const auto ui = runtime.settings().updateUiLanguage(
            applicationContext(), {.uiLanguage = QStringLiteral("en_US")});
        const auto singing = runtime.settings().updateSinging(
            applicationContext(), {.defaultLanguage = QStringLiteral("eng"),
                                   .defaultLyrics = QMap<QString, QString>{
                                       {QStringLiteral("eng"), QStringLiteral("la")}}});
        const auto theme = runtime.settings().updateTheme(applicationContext(),
                                                          {.themeId = QStringLiteral("dark")});
        const auto audio = runtime.settings().updateAudioDevice(applicationContext(),
                                                                {.gain = 0.75, .pan = -0.25});
        const auto playback =
            runtime.settings().updatePlaybackBehavior(applicationContext(), {.behavior = 1});
        const auto compute = runtime.settings().updateComputeDevice(
            applicationContext(), {.executionProvider = QStringLiteral("DirectML")});
        const auto render = runtime.settings().updateRender(
            applicationContext(), {.samplingSteps = 32, .runVocoderOnCpu = true});
        const auto retention = runtime.settings().updateSingerSessionRetention(
            applicationContext(), {.capacity = 2, .idleTimeoutSeconds = 120});
        check(ui && singing && theme && audio && playback && compute && render && retention,
              QStringLiteral("all public settings update domains must accept valid sparse updates"));
        check(compute.get().restartRequired &&
                  compute.get().restartRequiredFields.contains(
                      QStringLiteral("execution_provider")) &&
                  render.get().restartRequired,
              QStringLiteral("restart-only settings must report precise restart fields"));
        check(runtime.documentVersion() == version,
              QStringLiteral("application settings must not change document revision"));

        const auto beforeFailure = harness.settings;
        harness.settingsApplySucceeds = false;
        const auto failed = runtime.settings().updateTheme(applicationContext(),
                                                           {.themeId = QStringLiteral("light")});
        harness.settingsApplySucceeds = true;
        check(!failed && failed.getError().code == Automation::AutomationErrorCode::IoError &&
                  harness.settings == beforeFailure,
              QStringLiteral("settings persistence failure must roll back atomically"));
    }

    void testLyricRules(Automation::CoreRuntime &runtime, Harness &harness) {
        const auto initial = runtime.settings().listLyricRules();
        check(initial && initial.get().size() == 1 && initial.get().first().builtin,
              QStringLiteral("lyric_rules.list must include built-in rules with stable IDs"));

        const Automation::LyricRuleDraftDto draft{
            .kind = Automation::LyricRuleKind::Splitter,
            .name = QStringLiteral("punctuation"),
            .regexes = {QStringLiteral("([,，])")},
            .enabled = true,
            .position = 0,
        };
        const auto writesBefore = harness.lyricWrites;
        const auto preview = runtime.settings().createLyricRule(applicationContext(true), draft);
        check(preview && preview.get().validatedOnly && preview.get().rule.order == 0 &&
                  harness.lyricWrites == writesBefore,
              QStringLiteral("lyric rule create preview must be ordered and side-effect free"));
        const auto created = runtime.settings().createLyricRule(applicationContext(), draft);
        check(created && created.get().changed && !created.get().rule.ruleId.isEmpty() &&
                  harness.lyricWrites == writesBefore + 1,
              QStringLiteral("lyric rule create must persist once and return the created rule"));
        if (!created)
            return;
        const auto ruleId = created.get().rule.ruleId;

        const auto beforeInvalid = harness.settings.fillLyric;
        const auto wrongKind = runtime.settings().updateLyricRule(
            applicationContext(), ruleId, {.language = QStringLiteral("cmn")});
        check(!wrongKind &&
                  wrongKind.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
                  harness.settings.fillLyric == beforeInvalid,
              QStringLiteral("splitter updates must reject tagger-only fields atomically"));
        const auto invalid = runtime.settings().updateLyricRule(
            applicationContext(), ruleId, {.regexes = QStringList{QStringLiteral("(")}});
        check(!invalid &&
                  invalid.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
                  harness.settings.fillLyric == beforeInvalid,
              QStringLiteral("invalid lyric regex must fail atomically before persistence"));
        const auto renamed = runtime.settings().updateLyricRule(
            applicationContext(), ruleId, {.name = QStringLiteral("punctuation-v2")});
        const auto disabled =
            runtime.settings().setLyricRuleEnabled(applicationContext(), ruleId, false);
        const auto moved = runtime.settings().moveLyricRule(applicationContext(), ruleId, 1);
        check(renamed && renamed.get().rule.name == QStringLiteral("punctuation-v2") && disabled &&
                  !disabled.get().rule.enabled && moved && moved.get().rule.order == 1,
              QStringLiteral("lyric rule update, enable and move must return the effective rule"));

        const auto builtinUpdate =
            runtime.settings().updateLyricRule(applicationContext(), initial.get().first().ruleId,
                                               {.name = QStringLiteral("forbidden")});
        check(!builtinUpdate &&
                  builtinUpdate.getError().code == Automation::AutomationErrorCode::InvalidArgument,
              QStringLiteral("built-in lyric rule content must be immutable"));
        const auto tested = runtime.settings().testLyricRules(QStringLiteral("一闪一闪"));
        check(tested && tested.get().splitTokens == QStringList{QStringLiteral("一闪一闪")},
              QStringLiteral("lyric_rules.test must use the live pipeline without persistence"));
        const auto removed = runtime.settings().deleteLyricRule(applicationContext(), ruleId);
        check(removed && removed.get().ruleId == ruleId &&
                  runtime.settings().listLyricRules().get().size() == 1,
              QStringLiteral("lyric rule delete must return the removed stable ID"));

        const Automation::LyricRuleDraftDto taggerDraft{
            .kind = Automation::LyricRuleKind::Tagger,
            .name = QStringLiteral("latin-words"),
            .language = QStringLiteral("eng"),
            .entries = {{
                .type = QStringLiteral("array"),
                .value = {QStringLiteral("star")},
                .tag = QStringLiteral("word"),
            }},
        };
        const auto tagger = runtime.settings().createLyricRule(applicationContext(), taggerDraft);
        check(tagger && tagger.get().rule.name == QStringLiteral("latin-words") &&
                  harness.settings.fillLyric.customTaggerRules.size() == 1 &&
                  harness.settings.fillLyric.customTaggerRules.first().name ==
                      QStringLiteral("latin-words"),
              QStringLiteral("tagger rule names must be preserved by the settings DTO"));
        if (!tagger)
            return;
        const auto taggerId = tagger.get().rule.ruleId;
        const auto beforeWrongTaggerField = harness.settings.fillLyric;
        const auto wrongTaggerField = runtime.settings().updateLyricRule(
            applicationContext(), taggerId, {.regexes = QStringList{QStringLiteral("([a-z]+)")}});
        check(!wrongTaggerField &&
                  wrongTaggerField.getError().code ==
                      Automation::AutomationErrorCode::InvalidArgument &&
                  harness.settings.fillLyric == beforeWrongTaggerField,
              QStringLiteral("tagger updates must reject splitter-only fields atomically"));
        const auto renamedTagger = runtime.settings().updateLyricRule(
            applicationContext(), taggerId, {.name = QStringLiteral("latin-words-v2")});
        check(renamedTagger && renamedTagger.get().rule.name == QStringLiteral("latin-words-v2") &&
                  harness.settings.fillLyric.customTaggerRules.first().name ==
                      QStringLiteral("latin-words-v2"),
              QStringLiteral("tagger name updates must persist and round-trip"));

        auto missingDictionaryDraft = taggerDraft;
        missingDictionaryDraft.name = QStringLiteral("missing-dict");
        missingDictionaryDraft.language = QStringLiteral("und-x-missing");
        missingDictionaryDraft.entries = {
            {
             .type = QStringLiteral("dict"),
             .value = {QStringLiteral("missing-dictionary.txt")},
             .tag = QStringLiteral("word"),
             }
        };
        const auto beforeMissingDictionary = harness.settings.fillLyric;
        const auto writesBeforeMissingDictionary = harness.lyricWrites;
        const auto missingDictionary =
            runtime.settings().createLyricRule(applicationContext(), missingDictionaryDraft);
        check(!missingDictionary &&
                  missingDictionary.getError().code ==
                      Automation::AutomationErrorCode::InvalidArgument &&
                  harness.settings.fillLyric == beforeMissingDictionary &&
                  harness.lyricWrites == writesBeforeMissingDictionary,
              QStringLiteral("runtime-invalid tagger dictionaries must fail before persistence"));

        const auto removedTagger =
            runtime.settings().deleteLyricRule(applicationContext(), taggerId);
        check(removedTagger && runtime.settings().listLyricRules().get().size() == 1,
              QStringLiteral("custom tagger rules must remain deletable after updates"));
    }

    void testPackages(Automation::CoreRuntime &runtime, Harness &harness) {
        const auto projection = [](const QString &path) -> std::optional<QString> {
            if (path.startsWith(QStringLiteral("C:/allowed")))
                return path;
            return std::nullopt;
        };
        const auto packages = runtime.packages().getInstalledPackages(projection);
        check(packages && packages.get().size() == 2 && packages.get().first().path.isEmpty() &&
                  !packages.get().last().path.isEmpty(),
              QStringLiteral("packages.list must omit paths outside allowed read roots"));
        const auto described =
            runtime.packages().describePackage(QStringLiteral("voice.package"), projection);
        check(described && described.get().version == QVersionNumber(2, 0),
              QStringLiteral("packages.describe must deterministically select the newest version"));
        const auto describedVersion = runtime.packages().describePackage(
            QStringLiteral("voice.package"), QStringLiteral("1.0"), projection);
        check(describedVersion && describedVersion.get().version == QVersionNumber(1, 0) &&
                  describedVersion.get().path.isEmpty(),
              QStringLiteral("packages.describe must select and project an explicit version"));
        const auto invalidVersion = runtime.packages().describePackage(
            QStringLiteral("voice.package"), QStringLiteral("1..0"), projection);
        check(!invalidVersion &&
                  invalidVersion.getError().code ==
                      Automation::AutomationErrorCode::InvalidArgument &&
                  invalidVersion.getError().fieldPath == QStringLiteral("version"),
              QStringLiteral("packages.describe must reject malformed explicit versions"));

        bool previewCompleted = false;
        const auto preview = runtime.packages().refreshPackages(
            applicationContext(true),
            [&](const Automation::AutomationResult<Automation::PackageRefreshResultDto> &result) {
                previewCompleted =
                    result && result.get().packages == 2 && result.get().added.isEmpty();
            },
            projection);
        check(preview && previewCompleted && harness.refreshStarts == 0,
              QStringLiteral("packages.refresh validate_only must not start a filesystem scan"));

        bool refreshCompleted = false;
        const auto refreshed = runtime.packages().refreshPackages(
            applicationContext(),
            [&](const Automation::AutomationResult<Automation::PackageRefreshResultDto> &result) {
                refreshCompleted =
                    result &&
                    result.get().added == QStringList{QStringLiteral("new.package@1.0")} &&
                    result.get().failures.first().path.isEmpty();
            },
            projection);
        check(refreshed && refreshCompleted && harness.refreshStarts == 1,
              QStringLiteral("packages.refresh must start once and project failure paths"));
    }
}

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    Harness harness;
    AutomationTestSupport::TestRuntime testRuntime({}, {}, {}, {}, harness.packageServices(), {},
                                                   {}, harness.settingsServices());
    auto &runtime = testRuntime.runtime();
    testPublicSettings(runtime, harness);
    testLyricRules(runtime, harness);
    testPackages(runtime, harness);
    return failures == 0 ? 0 : 1;
}
