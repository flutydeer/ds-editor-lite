#ifndef PUBLICAUTOMATIONREGISTRY_H
#define PUBLICAUTOMATIONREGISTRY_H

#include "../AutomationTaskManager.h"
#include "../AutomationTypes.h"
#include "../ProjectAutomationDtos.h"
#include "AutomationAccessPolicy.h"
#include "AutomationFileGuard.h"
#include "AdmissionController.h"

#include <lite/AutomationWire/PublicToolContract.h>
#include <lite/AutomationWire/OpaqueCursorCodec.h>

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>

#include <functional>
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
    };

    struct PublicDocumentOpenRequest {
        CommandContext command;
        QString canonicalPath;
        QString formatId;
        QString encoding;
        bool importTempo = true;
        bool importTimeSignature = true;
        QString planDigest;
        PublicUnsavedPolicy unsavedPolicy = PublicUnsavedPolicy::Reject;
    };

    struct PublicDocumentImportRequest {
        CommandContext command;
        QString canonicalPath;
        QString formatId;
        QString encoding;
        bool importTempo = true;
        bool importTimeSignature = true;
        QString planDigest;
        QString mergeMode;
    };

    struct PublicDocumentBatchImportItem {
        QString canonicalPath;
        QString formatId;
        QJsonObject options;
        QString planDigest;
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

    struct PublicPreparedAudioPath {
        QString sha512;
        QJsonObject formatData;
    };

    struct PublicAutomationHostServices {
        QUuid editorInstanceId;
        QString hostMode = QStringLiteral("gui");
        std::function<QJsonArray()> documentStatus;
        std::function<QJsonArray()> windowStatus;

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
        std::function<AutomationResult<PublicPreparedAudioPath>(const QString &canonicalPath)>
            prepareAudioPath;
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

        [[nodiscard]] const QList<AutomationWire::ToolContract> &contracts() const;
        [[nodiscard]] QStringList bindingIds() const;
        [[nodiscard]] QList<AutomationWire::ToolContract> enabledContracts() const;
        [[nodiscard]] bool isComplete() const;

        AutomationResult<QJsonObject> invoke(const QString &operationId,
                                             const QJsonObject &arguments,
                                             const PublicInvocationContext &context = {});

    private:
        using Handler = std::function<AutomationResult<QJsonObject>(
            const QJsonObject &, const PublicInvocationContext &)>;

        void registerBindings();
        void addBinding(const QString &trackingId, Handler handler);
        void addOperationBinding(const QString &operationId, Handler handler);
        [[nodiscard]] const AutomationWire::ToolContract *
            contractForTracking(const QString &trackingId) const;
        AutomationResult<QJsonArray> resolveValueOptions(const AutomationWire::ToolContract &target,
                                                         const QString &fieldPath,
                                                         const QJsonObject &partialArguments);
        AutomationResult<AutomationUnit>
            validateDynamicArguments(const AutomationWire::ToolContract &target,
                                     const QJsonObject &arguments);

        CoreRuntime &m_runtime;
        AutomationAccessPolicy &m_accessPolicy;
        AutomationFileGuard &m_fileGuard;
        AdmissionController &m_admissionController;
        PublicAutomationHostServices m_hostServices;
        QHash<QString, Handler> m_handlers;
        AutomationWire::OpaqueCursorCodec m_manifestCursorCodec;
        AutomationWire::OpaqueCursorCodec m_taskCursorCodec;
        AutomationWire::OpaqueCursorCodec m_collectionCursorCodec;
    };

} // namespace Automation

#endif // PUBLICAUTOMATIONREGISTRY_H
