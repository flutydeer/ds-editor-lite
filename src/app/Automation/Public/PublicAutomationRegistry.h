#ifndef PUBLICAUTOMATIONREGISTRY_H
#define PUBLICAUTOMATIONREGISTRY_H

#include "../AutomationTaskManager.h"
#include "../AutomationTypes.h"
#include "../ProjectAutomationDtos.h"
#include "AutomationAccessPolicy.h"
#include "AutomationFileGuard.h"
#include "AdmissionController.h"

#include <lite/AutomationWire/PublicToolContract.h>
#include <lite/AutomationWire/PublicToolNames.h>
#include <lite/AutomationWire/OpaqueCursorCodec.h>

#include <QHash>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QLatin1StringView>
#include <QThreadPool>

#include <atomic>
#include <functional>
#include <memory>
#include <map>
#include <optional>

namespace Automation {

    class CoreRuntime;

    enum class PublicUnsavedPolicy {
        Reject,
        Discard,
    };

    enum class PublicBatchFailurePolicy {
        Atomic,
        BestEffort,
    };

    struct PublicInvocationContext {
        QString clientId;
        InvocationSource source = InvocationSource::PublicMcp;
        bool newWindow = false;
        bool alreadyOpen = false;
        TaskId pendingOpen;
    };

    using PublicProjectInputRevalidator =
        std::function<AutomationResult<std::optional<QByteArray>>()>;

    struct PublicDocumentOpenRequest {
        CommandContext command;
        QString canonicalPath;
        QString formatId;
        QString encoding;
        bool importTempo = true;
        bool importTimeSignature = true;
        QString planDigest;
        PublicProjectInputRevalidator revalidatePlan;
        PublicUnsavedPolicy unsavedPolicy = PublicUnsavedPolicy::Reject;
        bool newWindow = false;
    };

    struct PublicDocumentImportRequest {
        CommandContext command;
        QString canonicalPath;
        QString formatId;
        QString encoding;
        bool importTempo = true;
        bool importTimeSignature = true;
        QString planDigest;
        PublicProjectInputRevalidator revalidatePlan;
        QString mergeMode;
    };

    struct PublicDocumentBatchImportItem {
        QString canonicalPath;
        QString formatId;
        QJsonObject options;
        QString planDigest;
        PublicProjectInputRevalidator revalidatePlan;
        std::optional<AutomationError> validationError;
    };

    struct PublicDocumentBatchImportRequest {
        CommandContext command;
        QList<PublicDocumentBatchImportItem> items;
        PublicBatchFailurePolicy failurePolicy = PublicBatchFailurePolicy::Atomic;
    };

    struct PublicAudioClipProperties {
        std::optional<QString> name;
        std::optional<int> start;
        std::optional<int> length;
        std::optional<int> clipStart;
        std::optional<int> clipLength;
        std::optional<double> gain;
        std::optional<bool> mute;
    };

    struct PublicAudioClipImportRequest {
        CommandContext command;
        TrackId trackId;
        QString canonicalPath;
        std::optional<PublicAudioClipProperties> properties;
        QString clientRef;
    };

    struct PublicAudioClipBatchItem {
        TrackId trackId;
        QString canonicalPath;
        std::optional<PublicAudioClipProperties> properties;
        QString clientRef;
        std::optional<AutomationError> validationError;
    };

    struct PublicAudioClipBatchImportRequest {
        CommandContext command;
        QList<PublicAudioClipBatchItem> items;
        PublicBatchFailurePolicy failurePolicy = PublicBatchFailurePolicy::Atomic;
    };

    struct PublicInferenceStartRequest {
        CommandContext command;
        QJsonObject scope;
        QStringList stages;
        QJsonObject options;
    };

    struct PublicInferenceResetRequest {
        CommandContext command;
        QJsonObject scope;
        QString stage;
    };

    enum class PublicAudioPathUpdateMode {
        Relocate,
        Confirm,
    };

    struct PublicAudioPathUpdateRequest {
        CommandContext command;
        ClipId clipId;
        QString canonicalPath;
        PublicAudioPathUpdateMode mode = PublicAudioPathUpdateMode::Relocate;
        std::function<AutomationResult<AutomationUnit>()> authorizeCommit;
    };

    struct PublicAutomationHostServices {
        QUuid editorInstanceId;
        QString hostMode = QStringLiteral("gui");
        std::function<QJsonArray()> documentStatus;
        std::function<QJsonArray()> windowStatus;
        std::function<CoreRuntime *(const DocumentId &)> findDocument;
        std::function<CoreRuntime *(const QString &)> findOpenPath;
        std::function<CoreRuntime *()> createDocumentWindow;
        std::function<void(CoreRuntime &)> activateWindow;
        std::function<TaskId(CoreRuntime &, const QString &)> pendingOpen;
        std::function<void(CoreRuntime &, const QString &, const TaskId &)> trackOpen;
        std::function<AutomationResult<DocumentVersion>(CoreRuntime &, bool)> closeDocument;
        std::function<PublicAutomationHostServices(CoreRuntime &)> bindDocument;

        std::function<AutomationResult<TaskAcceptedResult>(const PublicDocumentOpenRequest &)>
            openDocument;
        std::function<AutomationResult<TaskAcceptedResult>(const PublicDocumentImportRequest &)>
            importDocument;
        std::function<AutomationResult<TaskAcceptedResult>(
            const PublicDocumentBatchImportRequest &)>
            importDocuments;
        std::function<AutomationResult<TaskAcceptedResult>(const PublicAudioClipImportRequest &)>
            importAudioClip;
        std::function<AutomationResult<TaskAcceptedResult>(
            const PublicAudioClipBatchImportRequest &)>
            importAudioClips;
        std::function<AutomationResult<TaskAcceptedResult>(const PublicAudioPathUpdateRequest &)>
            updateAudioClipPath;
        std::function<AutomationResult<QJsonValue>(const DocumentId &)> audioExportCapabilities;
        std::function<AutomationResult<QJsonValue>(const DocumentId &, ClipId)>
            extractionCapabilities;
        std::function<AutomationResult<QJsonValue>(const DocumentId &, const QJsonObject &)>
            inferenceCapabilities;
        std::function<AutomationResult<QJsonValue>(const DocumentId &, const QJsonObject &)>
            inferenceStatus;
        std::function<AutomationResult<TaskAcceptedResult>(const PublicInferenceStartRequest &)>
            startInference;
        std::function<AutomationResult<MutationResult>(const PublicInferenceResetRequest &)>
            resetInferenceStage;
    };

    class PublicAutomationRegistry final {
    public:
        PublicAutomationRegistry(CoreRuntime &runtime, AutomationAccessPolicy &accessPolicy,
                                 AutomationFileGuard &fileGuard,
                                 AdmissionController &admissionController,
                                 PublicAutomationHostServices hostServices = {});
        ~PublicAutomationRegistry();

        [[nodiscard]] const QList<AutomationWire::ToolContract> &contracts() const;
        [[nodiscard]] QStringList bindingIds() const;
        [[nodiscard]] QList<AutomationWire::ToolContract> enabledContracts() const;
        [[nodiscard]] bool isComplete() const;

        AutomationResult<QJsonObject> invoke(const QString &operationId,
                                             const QJsonObject &arguments,
                                             const PublicInvocationContext &context = {});

    private:
        struct LifetimeState {
            std::atomic_bool active{true};
        };

        using Handler = std::function<AutomationResult<QJsonObject>(
            const QJsonObject &, const PublicInvocationContext &)>;

        void registerBindings();
        AutomationResult<QJsonObject> invokeForTarget(const AutomationWire::ToolContract &contract,
                                                      const QJsonObject &arguments,
                                                      const PublicInvocationContext &context);
        PublicAutomationRegistry &bindingsFor(CoreRuntime &runtime);
        void registerAdvancedGuiBindings();
        void registerAdvancedApplicationBindings();
        void addBinding(QLatin1StringView toolName, Handler handler);
        std::function<AutomationResult<AutomationUnit>()>
            audioPathCommitGuard(AuthorizedPath authorizedPath) const;
        CoreRuntime &m_runtime;
        AutomationAccessPolicy &m_accessPolicy;
        AutomationFileGuard &m_fileGuard;
        AdmissionController &m_admissionController;
        std::shared_ptr<LifetimeState> m_lifetimeState = std::make_shared<LifetimeState>();
        PublicAutomationHostServices m_hostServices;
        QHash<QString, Handler> m_handlers;
        AutomationWire::OpaqueCursorCodec m_taskCursorCodec;
        AutomationWire::OpaqueCursorCodec m_collectionCursorCodec;
        QThreadPool m_exportThreadPool;
        QThreadPool *m_processExportPool = &m_exportThreadPool;
        std::map<CoreRuntime *, std::unique_ptr<PublicAutomationRegistry>> m_documentRegistries;
        QObject m_connections;
    };

} // namespace Automation

#endif // PUBLICAUTOMATIONREGISTRY_H
