#ifndef SETTINGSAUTOMATIONFACADE_H
#define SETTINGSAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"

#include <QByteArray>
#include <QJsonValue>
#include <QList>
#include <QMap>

#include <functional>
#include <optional>

namespace Automation {

    struct GeneralSettingsDto {
        QString uiLanguage;
        QString defaultSingingLanguage;
        QMap<QString, QString> defaultLyrics;
        QStringList packageSearchPaths;
        QStringList recentProjectFiles;
        QString gameDirectory;
        QString pitchModelPath;
        QString libreSvipPath;

        friend bool operator==(const GeneralSettingsDto &, const GeneralSettingsDto &) = default;
    };

    struct AppearanceSettingsDto {
        bool useNativeFrame = false;
        bool enableDirectManipulation = true;
        bool animationEnabled = true;
        double animationTimeScale = 1.0;
        QString themeId;
        QString uiFontFamily;

        friend bool operator==(const AppearanceSettingsDto &,
                               const AppearanceSettingsDto &) = default;
    };

    struct InferenceSettingsDto {
        QString executionProvider;
        int selectedGpuIndex = -1;
        QString selectedGpuId;
        int samplingSteps = 20;
        double depth = 1.0;
        bool runVocoderOnCpu = false;
        bool autoStartInference = true;
        double playbackLookaheadSeconds = 20.0;
        QString cacheDirectory;
        int singerSessionCacheCapacity = 4;
        int singerSessionIdleTimeoutSeconds = 60;
        int pitchSmoothKernelSize = 0;

        friend bool operator==(const InferenceSettingsDto &,
                               const InferenceSettingsDto &) = default;
    };

    enum class EditorRenderBackend {
        Legacy,
        RhiExperimental,
    };

    struct DeveloperSettingsDto {
        bool enableDiagnostics = false;
        bool showLogWindow = false;
        bool showTimelineDebugInfo = false;
        bool showClipDebugInfo = false;
        bool enablePanelDetach = false;
        bool enableEmbeddedOptionsDialog = false;
        EditorRenderBackend editorRenderBackend = EditorRenderBackend::Legacy;

        friend bool operator==(const DeveloperSettingsDto &,
                               const DeveloperSettingsDto &) = default;
    };

    struct G2pLanguageSettingsDto {
        QStringList languageOrder;

        friend bool operator==(const G2pLanguageSettingsDto &,
                               const G2pLanguageSettingsDto &) = default;
    };

    struct SplitterRuleDto {
        QString ruleId;
        QString name;
        QStringList regexes;
        bool enabled = true;
        int order = 0;

        friend bool operator==(const SplitterRuleDto &, const SplitterRuleDto &) = default;
    };

    struct TaggerEntryDto {
        QString type;
        QStringList value;
        QString tag;
        bool discard = false;

        friend bool operator==(const TaggerEntryDto &, const TaggerEntryDto &) = default;
    };

    struct TaggerRuleDto {
        QString ruleId;
        QString name;
        QString language;
        QList<TaggerEntryDto> entries;
        bool enabled = true;

        friend bool operator==(const TaggerRuleDto &, const TaggerRuleDto &) = default;
    };

    struct FillLyricSettingsDto {
        bool baseVisible = true;
        bool extensionVisible = false;
        int splitMode = 0;
        bool skipSlur = false;
        bool exportLanguage = false;
        double textEditFontSize = 11.0;
        double viewFontSize = 12.0;
        QMap<QString, bool> builtinSplitterEnabled;
        QMap<QString, bool> builtinTaggerEnabled;
        QList<SplitterRuleDto> customSplitterRules;
        QList<TaggerRuleDto> customTaggerRules;
        QStringList splitterOrder;
        QStringList taggerOrder;

        friend bool operator==(const FillLyricSettingsDto &,
                               const FillLyricSettingsDto &) = default;
    };

    struct WindowSettingsDto {
        QByteArray mainWindowGeometry;

        friend bool operator==(const WindowSettingsDto &, const WindowSettingsDto &) = default;
    };

    struct PseudoSingerSynthSettingsDto {
        int generator = 0;
        double amplitude = 0.0;
        int attackMilliseconds = 0;
        int decayMilliseconds = 0;
        double decayRatio = 0.0;
        int releaseMilliseconds = 0;

        friend bool operator==(const PseudoSingerSynthSettingsDto &,
                               const PseudoSingerSynthSettingsDto &) = default;
    };

    struct AudioExportConfigDto {
        QString fileName;
        QString fileDirectory;
        int fileType = 0;
        bool mono = false;
        int formatOption = 0;
        int formatQuality = 100;
        double sampleRate = 44100.0;
        int mixingOption = 0;
        bool muteSoloEnabled = true;
        int sourceOption = 0;
        QList<int> sources;
        int timeRange = 0;

        friend bool operator==(const AudioExportConfigDto &,
                               const AudioExportConfigDto &) = default;
    };

    struct AudioExportPresetDto {
        QString name;
        AudioExportConfigDto config;

        friend bool operator==(const AudioExportPresetDto &,
                               const AudioExportPresetDto &) = default;
    };

    struct AudioSettingsDto {
        qint64 adoptedBufferSize = 0;
        double adoptedSampleRate = 0.0;
        double deviceGain = 1.0;
        QString deviceName;
        double devicePan = 0.0;
        QString driverName;
        qint64 fileBufferingReadAheadSize = 0;
        int hotPlugNotificationMode = 0;
        int playheadBehavior = 0;
        int midiDeviceIndex = -1;
        double midiSynthesizerAmplitude = -9.0;
        int midiSynthesizerAttackMilliseconds = 10;
        int midiSynthesizerDecayMilliseconds = 1000;
        double midiSynthesizerDecayRatio = 0.5;
        double midiSynthesizerFrequencyOfA = 0.0;
        int midiSynthesizerGenerator = 3;
        int midiSynthesizerReleaseMilliseconds = 50;
        bool pseudoSingerReadEnergy = true;
        bool pseudoSingerReadPitch = true;
        QList<PseudoSingerSynthSettingsDto> pseudoSingerSynthesizers;
        int vstEditorPort = 28081;
        bool vstPluginEditorUsesCustomTheme = false;
        int vstPluginPort = 28082;
        bool audioExporterClippingCheckEnabled = true;
        int audioExporterIgnoredWarningFlags = 0;
        bool audioExporterUseTemporaryFile = true;
        QList<AudioExportPresetDto> audioExporterPresets;
        QString currentAudioExporterPreset;
        int legacyAudioExporterPresetIndex = 0;
        bool currentAudioExporterPresetIsName = false;

        friend bool operator==(const AudioSettingsDto &, const AudioSettingsDto &) = default;
    };

    struct SettingsSnapshotDto {
        GeneralSettingsDto general;
        AppearanceSettingsDto appearance;
        InferenceSettingsDto inference;
        DeveloperSettingsDto developer;
        G2pLanguageSettingsDto g2pLanguage;
        FillLyricSettingsDto fillLyric;
        WindowSettingsDto window;
        AudioSettingsDto audio;

        friend bool operator==(const SettingsSnapshotDto &, const SettingsSnapshotDto &) = default;
    };

    struct SettingsStringCandidateDto {
        QString id;
        QString displayName;
        bool available = true;
        QString unavailableReason;

        friend bool operator==(const SettingsStringCandidateDto &,
                               const SettingsStringCandidateDto &) = default;
    };

    struct SettingsGpuCandidateDto {
        int index = -1;
        QString id;
        QString displayName;
        bool available = true;
        QString unavailableReason;

        friend bool operator==(const SettingsGpuCandidateDto &,
                               const SettingsGpuCandidateDto &) = default;
    };

    struct SettingsNumericRangeDto {
        double minimum = 0.0;
        double maximum = 0.0;
        double step = 0.0;

        friend bool operator==(const SettingsNumericRangeDto &,
                               const SettingsNumericRangeDto &) = default;
    };

    struct UiLanguagePublicSettingsDto {
        QString configured;
        QString effective;
        QList<SettingsStringCandidateDto> candidates;
        QString unavailableReason;

        friend bool operator==(const UiLanguagePublicSettingsDto &,
                               const UiLanguagePublicSettingsDto &) = default;
    };

    struct SingingPublicSettingsDto {
        QString configuredDefaultLanguage;
        QString effectiveDefaultLanguage;
        QMap<QString, QString> configuredDefaultLyrics;
        QMap<QString, QString> effectiveDefaultLyrics;
        QList<SettingsStringCandidateDto> languageCandidates;
        QString unavailableReason;

        friend bool operator==(const SingingPublicSettingsDto &,
                               const SingingPublicSettingsDto &) = default;
    };

    struct ThemePublicSettingsDto {
        QString configured;
        QString effective;
        QList<SettingsStringCandidateDto> candidates;
        QString unavailableReason;

        friend bool operator==(const ThemePublicSettingsDto &,
                               const ThemePublicSettingsDto &) = default;
    };

    struct AudioDevicePublicValueDto {
        QString driverName;
        QString deviceName;
        qint64 bufferSize = 0;
        double sampleRate = 0.0;
        int hotPlugNotificationMode = 0;
        double gain = 1.0;
        double pan = 0.0;

        friend bool operator==(const AudioDevicePublicValueDto &,
                               const AudioDevicePublicValueDto &) = default;
    };

    struct AudioDeviceCandidateDto {
        QString id;
        QString displayName;
        QList<qint64> bufferSizes;
        QList<double> sampleRates;
        bool available = true;
        QString unavailableReason;

        friend bool operator==(const AudioDeviceCandidateDto &,
                               const AudioDeviceCandidateDto &) = default;
    };

    struct AudioDriverCandidateDto {
        QString id;
        QString displayName;
        QList<AudioDeviceCandidateDto> devices;
        bool available = true;
        QString unavailableReason;

        friend bool operator==(const AudioDriverCandidateDto &,
                               const AudioDriverCandidateDto &) = default;
    };

    struct AudioDevicePublicSettingsDto {
        AudioDevicePublicValueDto configured;
        AudioDevicePublicValueDto effective;
        QList<AudioDriverCandidateDto> drivers;
        QString unavailableReason;

        friend bool operator==(const AudioDevicePublicSettingsDto &,
                               const AudioDevicePublicSettingsDto &) = default;
    };

    struct PlaybackBehaviorPublicSettingsDto {
        int configured = 0;
        int effective = 0;
        QList<int> candidates;
        QString unavailableReason;

        friend bool operator==(const PlaybackBehaviorPublicSettingsDto &,
                               const PlaybackBehaviorPublicSettingsDto &) = default;
    };

    struct ComputeDevicePublicValueDto {
        QString executionProvider;
        int gpuIndex = -1;
        QString gpuId;

        friend bool operator==(const ComputeDevicePublicValueDto &,
                               const ComputeDevicePublicValueDto &) = default;
    };

    struct ComputeDevicePublicSettingsDto {
        ComputeDevicePublicValueDto configured;
        ComputeDevicePublicValueDto effective;
        QList<SettingsStringCandidateDto> providerCandidates;
        QList<SettingsGpuCandidateDto> gpuCandidates;
        QStringList restartRequiredFields;
        QString unavailableReason;

        friend bool operator==(const ComputeDevicePublicSettingsDto &,
                               const ComputeDevicePublicSettingsDto &) = default;
    };

    struct RenderPublicValueDto {
        int samplingSteps = 20;
        double depth = 1.0;
        bool runVocoderOnCpu = false;
        bool autoStartInference = true;
        double playbackLookaheadSeconds = 20.0;
        int pitchSmoothKernelSize = 0;

        friend bool operator==(const RenderPublicValueDto &,
                               const RenderPublicValueDto &) = default;
    };

    struct RenderPublicSettingsDto {
        RenderPublicValueDto configured;
        RenderPublicValueDto effective;
        SettingsNumericRangeDto samplingStepsRange;
        SettingsNumericRangeDto depthRange;
        SettingsNumericRangeDto playbackLookaheadRange;
        SettingsNumericRangeDto pitchSmoothKernelRange;
        QStringList restartRequiredFields;
        QString unavailableReason;

        friend bool operator==(const RenderPublicSettingsDto &,
                               const RenderPublicSettingsDto &) = default;
    };

    struct SingerSessionRetentionPublicValueDto {
        int capacity = 4;
        int idleTimeoutSeconds = 60;

        friend bool operator==(const SingerSessionRetentionPublicValueDto &,
                               const SingerSessionRetentionPublicValueDto &) = default;
    };

    struct SingerSessionRetentionPublicSettingsDto {
        SingerSessionRetentionPublicValueDto configured;
        SingerSessionRetentionPublicValueDto effective;
        QList<int> capacityCandidates;
        QList<int> idleTimeoutCandidates;
        QString unavailableReason;

        friend bool operator==(const SingerSessionRetentionPublicSettingsDto &,
                               const SingerSessionRetentionPublicSettingsDto &) = default;
    };

    struct PackageSearchPathsPublicSettingsDto {
        QStringList configured;
        QStringList effective;
        bool restartRequired = false;
        QString unavailableReason;

        friend bool operator==(const PackageSearchPathsPublicSettingsDto &,
                               const PackageSearchPathsPublicSettingsDto &) = default;
    };

    struct PublicSettingsSnapshotDto {
        std::optional<UiLanguagePublicSettingsDto> uiLanguage;
        std::optional<SingingPublicSettingsDto> singing;
        std::optional<ThemePublicSettingsDto> theme;
        std::optional<AudioDevicePublicSettingsDto> audioDevice;
        std::optional<PlaybackBehaviorPublicSettingsDto> playbackBehavior;
        std::optional<ComputeDevicePublicSettingsDto> computeDevice;
        std::optional<RenderPublicSettingsDto> render;
        std::optional<SingerSessionRetentionPublicSettingsDto> singerSessionRetention;
        std::optional<PackageSearchPathsPublicSettingsDto> packageSearchPaths;

        friend bool operator==(const PublicSettingsSnapshotDto &,
                               const PublicSettingsSnapshotDto &) = default;
    };

    struct UiLanguageSettingsPatchDto {
        std::optional<QString> uiLanguage;
    };

    struct SingingSettingsPatchDto {
        std::optional<QString> defaultLanguage;
        std::optional<QMap<QString, QString>> defaultLyrics;
    };

    struct ThemeSettingsPatchDto {
        std::optional<QString> themeId;
    };

    struct AudioDeviceSettingsPatchDto {
        std::optional<QString> driverName;
        std::optional<QString> deviceName;
        std::optional<qint64> bufferSize;
        std::optional<double> sampleRate;
        std::optional<int> hotPlugNotificationMode;
        std::optional<double> gain;
        std::optional<double> pan;
    };

    struct PlaybackBehaviorSettingsPatchDto {
        std::optional<int> behavior;
    };

    struct ComputeDeviceSettingsPatchDto {
        std::optional<QString> executionProvider;
        std::optional<int> gpuIndex;
        std::optional<QString> gpuId;
    };

    struct RenderSettingsPatchDto {
        std::optional<int> samplingSteps;
        std::optional<double> depth;
        std::optional<bool> runVocoderOnCpu;
        std::optional<bool> autoStartInference;
        std::optional<double> playbackLookaheadSeconds;
        std::optional<int> pitchSmoothKernelSize;
    };

    struct SingerSessionRetentionSettingsPatchDto {
        std::optional<int> capacity;
        std::optional<int> idleTimeoutSeconds;
    };

    struct PackageSearchPathsSettingsPatchDto {
        std::optional<QStringList> paths;
    };

    struct SettingsMutationResultDto {
        bool changed = false;
        bool validatedOnly = false;
        bool restartRequired = false;
        QStringList restartRequiredFields;

        friend bool operator==(const SettingsMutationResultDto &,
                               const SettingsMutationResultDto &) = default;
    };

    enum class LyricRuleKind {
        Splitter,
        Tagger,
    };

    struct LyricRuleDto {
        QString ruleId;
        LyricRuleKind kind = LyricRuleKind::Splitter;
        bool builtin = false;
        QString name;
        QString language;
        QStringList regexes;
        QList<TaggerEntryDto> entries;
        bool enabled = true;
        int order = 0;
        QString engineOrderKey;

        friend bool operator==(const LyricRuleDto &, const LyricRuleDto &) = default;
    };

    struct LyricRuleDraftDto {
        LyricRuleKind kind = LyricRuleKind::Splitter;
        QString name;
        QString language;
        QStringList regexes;
        QList<TaggerEntryDto> entries;
        bool enabled = true;
        std::optional<int> position;
    };

    struct LyricRulePatchDto {
        std::optional<QString> name;
        std::optional<QString> language;
        std::optional<QStringList> regexes;
        std::optional<QList<TaggerEntryDto>> entries;
    };

    struct LyricRuleTestTokenDto {
        QString lyric;
        QString language;
        QString tag;
        bool discard = false;

        friend bool operator==(const LyricRuleTestTokenDto &,
                               const LyricRuleTestTokenDto &) = default;
    };

    struct LyricRuleTestResultDto {
        QStringList splitTokens;
        QList<LyricRuleTestTokenDto> taggedTokens;

        friend bool operator==(const LyricRuleTestResultDto &,
                               const LyricRuleTestResultDto &) = default;
    };

    struct LyricRuleMutationResultDto {
        bool changed = false;
        bool validatedOnly = false;
        QStringList warnings;
        LyricRuleDto rule;

        friend bool operator==(const LyricRuleMutationResultDto &,
                               const LyricRuleMutationResultDto &) = default;
    };

    struct LyricRuleDeleteResultDto {
        bool changed = false;
        bool validatedOnly = false;
        QStringList warnings;
        QString ruleId;

        friend bool operator==(const LyricRuleDeleteResultDto &,
                               const LyricRuleDeleteResultDto &) = default;
    };

    using SettingsPathProjection =
        std::function<std::optional<QString>(const QString &canonicalPath)>;

    struct SettingsRuntimeServices {
        std::function<SettingsSnapshotDto()> snapshot;
        std::function<PublicSettingsSnapshotDto()> publicSnapshot;
        std::function<bool(const GeneralSettingsDto &)> applyGeneral;
        std::function<bool(const AppearanceSettingsDto &)> applyAppearance;
        std::function<bool(const InferenceSettingsDto &)> applyInference;
        std::function<bool(const DeveloperSettingsDto &)> applyDeveloper;
        std::function<bool(const G2pLanguageSettingsDto &)> applyG2pLanguage;
        std::function<bool(const FillLyricSettingsDto &)> applyFillLyric;
        std::function<AutomationResult<AutomationUnit>(const FillLyricSettingsDto &)>
            validateFillLyricRuntime;
        std::function<bool(const WindowSettingsDto &)> applyWindow;
        std::function<bool(const AudioSettingsDto &)> applyAudio;
        std::function<AutomationResult<AutomationUnit>(const GeneralSettingsDto &)> applyUiLanguage;
        std::function<AutomationResult<AutomationUnit>(const AppearanceSettingsDto &)> applyTheme;
        std::function<AutomationResult<AutomationUnit>(const AudioSettingsDto &,
                                                       const AudioDeviceSettingsPatchDto &)>
            applyAudioDevice;
        std::function<QList<LyricRuleDto>()> lyricRules;
        std::function<AutomationResult<LyricRuleTestResultDto>(const QString &)> testLyricRules;
    };

    class SettingsAutomationFacade final {
    public:
        SettingsAutomationFacade(AutomationDispatcher &dispatcher,
                                 SettingsRuntimeServices services = {});

        AutomationResult<SettingsSnapshotDto> getSettings();
        AutomationResult<PublicSettingsSnapshotDto>
            queryPublicSettings(QStringList domains = {},
                                SettingsPathProjection pathProjection = {});
        static QStringList publicSettingsDomains();

        AutomationResult<SettingsMutationResultDto>
            updateUiLanguage(const ApplicationCommandContext &context,
                             const UiLanguageSettingsPatchDto &patch);
        AutomationResult<SettingsMutationResultDto>
            updateSinging(const ApplicationCommandContext &context,
                          const SingingSettingsPatchDto &patch);
        AutomationResult<SettingsMutationResultDto>
            updateTheme(const ApplicationCommandContext &context,
                        const ThemeSettingsPatchDto &patch);
        AutomationResult<SettingsMutationResultDto>
            updateAudioDevice(const ApplicationCommandContext &context,
                              const AudioDeviceSettingsPatchDto &patch);
        AutomationResult<SettingsMutationResultDto>
            updatePlaybackBehavior(const ApplicationCommandContext &context,
                                   const PlaybackBehaviorSettingsPatchDto &patch);
        AutomationResult<SettingsMutationResultDto>
            updateComputeDevice(const ApplicationCommandContext &context,
                                const ComputeDeviceSettingsPatchDto &patch);
        AutomationResult<SettingsMutationResultDto>
            updateRender(const ApplicationCommandContext &context,
                         const RenderSettingsPatchDto &patch);
        AutomationResult<SettingsMutationResultDto>
            updateSingerSessionRetention(const ApplicationCommandContext &context,
                                         const SingerSessionRetentionSettingsPatchDto &patch);
        AutomationResult<SettingsMutationResultDto>
            updatePublicPackageSearchPaths(const ApplicationCommandContext &context,
                                           const PackageSearchPathsSettingsPatchDto &patch);

        AutomationResult<QList<LyricRuleDto>> listLyricRules();
        AutomationResult<LyricRuleMutationResultDto>
            createLyricRule(const ApplicationCommandContext &context,
                            const LyricRuleDraftDto &draft);
        AutomationResult<LyricRuleMutationResultDto>
            updateLyricRule(const ApplicationCommandContext &context, const QString &ruleId,
                            const LyricRulePatchDto &patch);
        AutomationResult<LyricRuleDeleteResultDto>
            deleteLyricRule(const ApplicationCommandContext &context, const QString &ruleId);
        AutomationResult<LyricRuleMutationResultDto>
            setLyricRuleEnabled(const ApplicationCommandContext &context, const QString &ruleId,
                                bool enabled);
        AutomationResult<LyricRuleMutationResultDto>
            moveLyricRule(const ApplicationCommandContext &context, const QString &ruleId,
                          int targetIndex);
        AutomationResult<LyricRuleTestResultDto> testLyricRules(const QString &text);
        AutomationResult<ApplicationMutationResult>
            updateGeneral(const ApplicationCommandContext &context,
                          const GeneralSettingsDto &settings);
        AutomationResult<ApplicationMutationResult>
            updateAppearance(const ApplicationCommandContext &context,
                             const AppearanceSettingsDto &settings);
        AutomationResult<ApplicationMutationResult>
            updateInference(const ApplicationCommandContext &context,
                            const InferenceSettingsDto &settings);
        AutomationResult<ApplicationMutationResult>
            updateDeveloper(const ApplicationCommandContext &context,
                            const DeveloperSettingsDto &settings);
        AutomationResult<ApplicationMutationResult>
            updateG2pLanguage(const ApplicationCommandContext &context,
                              const G2pLanguageSettingsDto &settings);
        AutomationResult<ApplicationMutationResult>
            updateFillLyric(const ApplicationCommandContext &context,
                            const FillLyricSettingsDto &settings);
        AutomationResult<ApplicationMutationResult>
            updateWindow(const ApplicationCommandContext &context,
                         const WindowSettingsDto &settings);
        AutomationResult<ApplicationMutationResult>
            updateAudio(const ApplicationCommandContext &context, const AudioSettingsDto &settings);

        AutomationResult<QStringList> getRecentProjectFiles();
        AutomationResult<ApplicationMutationResult>
            addRecentProjectFile(const ApplicationCommandContext &context, const QString &path);
        AutomationResult<ApplicationMutationResult>
            removeRecentProjectFile(const ApplicationCommandContext &context, const QString &path);
        AutomationResult<ApplicationMutationResult>
            clearRecentProjectFiles(const ApplicationCommandContext &context);

        AutomationResult<QStringList> getPackageSearchPaths();
        AutomationResult<ApplicationMutationResult>
            setPackageSearchPaths(const ApplicationCommandContext &context, QStringList paths);

    private:
        using GeneralMutation = std::function<void(GeneralSettingsDto &)>;
        using PublicSettingsMutation =
            std::function<AutomationResult<AutomationUnit>(SettingsSnapshotDto &)>;
        using PublicSettingsValidator =
            std::function<AutomationResult<AutomationUnit>(const SettingsSnapshotDto &)>;
        using PublicSettingsApply =
            std::function<AutomationResult<AutomationUnit>(const SettingsSnapshotDto &)>;
        using RestartFieldResolver =
            std::function<QStringList(const SettingsSnapshotDto &, const SettingsSnapshotDto &)>;

        template <typename T, typename Getter, typename Validator, typename Apply>
        AutomationResult<ApplicationMutationResult>
            update(const OperationId &operationId, const ApplicationCommandContext &context,
                   const T &settings, Getter getter, Validator validator, Apply apply);
        AutomationResult<ApplicationMutationResult>
            updateGeneralState(const OperationId &operationId,
                               const ApplicationCommandContext &context, GeneralMutation mutation,
                               std::optional<AutomationError> validationError = std::nullopt);
        AutomationResult<SettingsMutationResultDto> updatePublicSettings(
            const OperationId &operationId, const ApplicationCommandContext &context,
            PublicSettingsMutation mutation, PublicSettingsValidator validator,
            PublicSettingsApply apply, RestartFieldResolver restartFields = {});
        AutomationResult<AutomationUnit>
            validateFillLyricTarget(const FillLyricSettingsDto &target) const;


        AutomationDispatcher &m_dispatcher;
        SettingsRuntimeServices m_services;
    };

} // namespace Automation

#endif // SETTINGSAUTOMATIONFACADE_H
