#ifndef SETTINGSAUTOMATIONFACADE_H
#define SETTINGSAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"

#include <QByteArray>
#include <QList>
#include <QMap>

#include <functional>

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

    struct SettingsRuntimeServices {
        std::function<SettingsSnapshotDto()> snapshot;
        std::function<bool(const GeneralSettingsDto &)> applyGeneral;
        std::function<bool(const AppearanceSettingsDto &)> applyAppearance;
        std::function<bool(const InferenceSettingsDto &)> applyInference;
        std::function<bool(const DeveloperSettingsDto &)> applyDeveloper;
        std::function<bool(const G2pLanguageSettingsDto &)> applyG2pLanguage;
        std::function<bool(const FillLyricSettingsDto &)> applyFillLyric;
        std::function<bool(const WindowSettingsDto &)> applyWindow;
        std::function<bool(const AudioSettingsDto &)> applyAudio;
    };

    class SettingsAutomationFacade final {
    public:
        SettingsAutomationFacade(OperationCatalog &catalog,
                                 AutomationDispatcher &dispatcher,
                                 SettingsRuntimeServices services = {});

        AutomationResult<SettingsSnapshotDto> getSettings();
        AutomationResult<ApplicationMutationResult>
        updateGeneral(const ApplicationCommandContext &context, const GeneralSettingsDto &settings);
        AutomationResult<ApplicationMutationResult> updateAppearance(
            const ApplicationCommandContext &context, const AppearanceSettingsDto &settings);
        AutomationResult<ApplicationMutationResult> updateInference(
            const ApplicationCommandContext &context, const InferenceSettingsDto &settings);
        AutomationResult<ApplicationMutationResult> updateDeveloper(
            const ApplicationCommandContext &context, const DeveloperSettingsDto &settings);
        AutomationResult<ApplicationMutationResult> updateG2pLanguage(
            const ApplicationCommandContext &context, const G2pLanguageSettingsDto &settings);
        AutomationResult<ApplicationMutationResult> updateFillLyric(
            const ApplicationCommandContext &context, const FillLyricSettingsDto &settings);
        AutomationResult<ApplicationMutationResult>
        updateWindow(const ApplicationCommandContext &context, const WindowSettingsDto &settings);
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
        template <typename T, typename Getter, typename Validator, typename Apply>
        AutomationResult<ApplicationMutationResult>
        update(const OperationId &operationId, const ApplicationCommandContext &context,
               const T &settings, Getter getter, Validator validator, Apply apply);

        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        SettingsRuntimeServices m_services;
    };

} // namespace Automation

#endif // SETTINGSAUTOMATIONFACADE_H
