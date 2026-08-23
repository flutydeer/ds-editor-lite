#ifndef RUNTIMEDIMENSIONSUPPORT_H
#define RUNTIMEDIMENSIONSUPPORT_H

#include "Automation/CoreRuntime.h"
#include "Automation/OperationIds.h"
#include "Automation/ProjectAutomationDtos.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QMap>
#include <QSet>
#include <QStringList>
#include <QTextStream>

#include <functional>
#include <memory>
#include <optional>

namespace RuntimeDimensions {

    enum CoverageDimension : quint32 {
        NormalData = 1U << 0,
        SnapshotNoSideEffect = 1U << 1,
        ValidateOnlyOrNoOp = 1U << 2,
        UnknownTargetOrRevision = 1U << 3,
        HostUnavailable = 1U << 4,
        BoundaryOrUnicode = 1U << 5,
        PersistenceOrSingleHost = 1U << 6,
        IdempotencyOrRepeat = 1U << 7,
    };

    using CoverageDimensions = quint32;
    using RuntimeDimensionMatrix = QMap<Automation::OperationId, CoverageDimensions>;

    class ScenarioLog final {
    public:
        template <typename Function>
        void run(const Automation::OperationId &operationId, const QString &dimension,
                 Function &&function) {
            begin(operationId, dimension);
            std::forward<Function>(function)();
        }

        bool expect(bool condition, const QString &message);

        template <typename T>
        bool expectError(const Automation::AutomationResult<T> &result,
                         Automation::AutomationErrorCode code,
                         const Automation::OperationId &operationId, const QString &message) {
            return expect(!result && result.getError().code == code &&
                              result.getError().operationId == operationId,
                          message);
        }

        int finish(const RuntimeDimensionMatrix &expectedDimensions);

    private:
        void begin(const Automation::OperationId &operationId, const QString &dimension);

        QString m_currentScenario;
        QMap<QString, CoverageDimensions> m_operationDimensions;
        QMap<QString, int> m_operationScenarios;
        QSet<QString> m_scenarioIds;
        int m_scenarios = 0;
        int m_assertions = 0;
        int m_failures = 0;
    };

    struct HarnessOptions {
        Automation::OperationId missingOperation;
    };

    struct EditorObjects {
        Automation::TrackId trackId;
        Automation::ClipId clipId;
        Automation::NoteId noteId;
    };

    Automation::SettingsSnapshotDto validSettings();
    EditorViewState validViewState();
    Automation::SpeakerMixPresetDto validPreset(const QString &name = QStringLiteral("Lead"));

    class RuntimeHarness final {
    public:
        explicit RuntimeHarness(HarnessOptions options = {});
        ~RuntimeHarness();

        RuntimeHarness(const RuntimeHarness &) = delete;
        RuntimeHarness &operator=(const RuntimeHarness &) = delete;

        Automation::CoreRuntime &core();
        const Automation::CoreRuntime &core() const;
        void resetHistory();

        bool applicationTerminationSucceeds = true;
        bool playbackPlaySucceeds = true;
        bool playbackCanStart = true;
        bool editorViewAvailable = true;
        bool editorApplySucceeds = true;
        bool editorRevealSucceeds = true;
        bool settingsApplySucceeds = true;
        bool presetApplySucceeds = true;
        bool packageValidationSucceeds = true;

        Automation::ApplicationInfoDto applicationInfo;
        Automation::ApplicationTerminationMode lastTerminationMode =
            Automation::ApplicationTerminationMode::Exit;
        Automation::PlaybackHostSnapshot playback;
        EditorViewState editorView;
        Automation::EditorStableState editorStable;
        Automation::SettingsSnapshotDto settings;
        QList<Automation::SpeakerMixPresetDto> presets;
        QList<Automation::PackageDto> packages;
        Automation::PackageValidationReportDto packageReport;
        QString lastValidatedPackagePath;
        int packageResolveCount = 1;

        QMap<QString, int> hostCalls;
        int settingsWriteAttempts = 0;
        int settingsWrites = 0;
        int presetWriteAttempts = 0;
        int presetWrites = 0;
        int packageResolvePreviewCalls = 0;
        int packageResolveApplyCalls = 0;

    private:
        static HistoryManager *resetGlobalHistory();
        bool missing(const Automation::OperationId &operationId) const;

        template <typename T>
        bool persistSetting(const Automation::OperationId &operationId, T &target, const T &value) {
            ++hostCalls[operationId];
            ++settingsWriteAttempts;
            if (!settingsApplySucceeds)
                return false;
            target = value;
            ++settingsWrites;
            return true;
        }

        Automation::ApplicationRuntimeServices applicationServices();
        Automation::PlaybackRuntimeServices playbackServices();
        Automation::EditorRuntimeServices editorServices();
        Automation::SettingsRuntimeServices settingsServices();
        Automation::PresetRuntimeServices presetServices();
        Automation::PackageRuntimeServices packageServices();

        HarnessOptions m_options;
        AppModel m_model;
        HistoryManager *m_history;
        std::unique_ptr<Automation::CoreRuntime> m_runtime;
    };

    Automation::CommandContext commandContext(const RuntimeHarness &harness,
                                              bool validateOnly = false,
                                              const QString &idempotencyKey = {});
    Automation::GuiCommandContext guiContext(const RuntimeHarness &harness,
                                             bool validateOnly = false);
    Automation::GuiDocumentCommandContext guiDocumentContext(const RuntimeHarness &harness,
                                                             bool validateOnly = false);
    Automation::ApplicationCommandContext applicationContext(bool validateOnly = false);

    std::optional<EditorObjects> createEditorObjects(RuntimeHarness &harness);

    void runApplicationPlaybackDimensions(ScenarioLog &log);
    void runEditorDimensions(ScenarioLog &log);
    void runSettingsPackageDimensions(ScenarioLog &log);
    RuntimeDimensionMatrix expectedRuntimeDimensions();

} // namespace RuntimeDimensions

#endif // RUNTIMEDIMENSIONSUPPORT_H
