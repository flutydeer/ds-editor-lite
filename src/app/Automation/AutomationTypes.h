#ifndef AUTOMATIONTYPES_H
#define AUTOMATIONTYPES_H

#include <lite/ADT/Expected.h>

#include <QByteArray>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QUuid>

#include <optional>
#include <utility>

namespace Automation {

    template <typename Tag>
    class StrongUuid {
    public:
        StrongUuid() = default;

        explicit StrongUuid(QUuid value) : m_value(std::move(value)) {
        }

        [[nodiscard]] static StrongUuid create() {
            return StrongUuid(QUuid::createUuid());
        }

        [[nodiscard]] static StrongUuid fromString(const QString &value) {
            return StrongUuid(QUuid::fromString(value));
        }

        [[nodiscard]] bool isNull() const {
            return m_value.isNull();
        }

        [[nodiscard]] const QUuid &value() const {
            return m_value;
        }

        [[nodiscard]] QString toString() const {
            return m_value.toString(QUuid::WithoutBraces);
        }

        friend bool operator==(const StrongUuid &, const StrongUuid &) = default;

        friend size_t qHash(const StrongUuid &id, const size_t seed = 0) noexcept {
            return ::qHash(id.m_value, seed);
        }

    private:
        QUuid m_value;
    };

    template <typename Tag>
    class StrongObjectId {
    public:
        StrongObjectId() = default;

        explicit StrongObjectId(const int value) : m_value(value) {
        }

        [[nodiscard]] bool isValid() const {
            return m_value >= 0;
        }

        [[nodiscard]] int value() const {
            return m_value;
        }

        friend bool operator==(const StrongObjectId &, const StrongObjectId &) = default;

        friend size_t qHash(const StrongObjectId &id, const size_t seed = 0) noexcept {
            return ::qHash(id.m_value, seed);
        }

    private:
        int m_value = -1;
    };

    struct DocumentIdTag;
    struct WindowIdTag;
    struct TaskIdTag;
    struct TrackIdTag;
    struct ClipIdTag;
    struct NoteIdTag;
    struct PieceIdTag;
    struct CurveIdTag;
    struct AnchorIdTag;
    struct SpeakerMixKeyframeIdTag;

    using DocumentId = StrongUuid<DocumentIdTag>;
    using WindowId = StrongUuid<WindowIdTag>;
    using TaskId = StrongUuid<TaskIdTag>;
    using TrackId = StrongObjectId<TrackIdTag>;
    using ClipId = StrongObjectId<ClipIdTag>;
    using NoteId = StrongObjectId<NoteIdTag>;
    using PieceId = StrongObjectId<PieceIdTag>;
    using CurveId = StrongObjectId<CurveIdTag>;
    using AnchorId = StrongObjectId<AnchorIdTag>;
    using SpeakerMixKeyframeId = StrongObjectId<SpeakerMixKeyframeIdTag>;
    using Revision = quint64;
    using OperationId = QString;

    struct DocumentVersion {
        DocumentId documentId;
        Revision revision = 0;

        friend bool operator==(const DocumentVersion &, const DocumentVersion &) = default;
    };

    enum class InvocationSource {
        TrustedGui,
        InternalAutomation,
        PublicMcp,
        Test,
    };

    struct CommandContext {
        DocumentVersion expected;
        bool validateOnly = false;
        QString idempotencyKey;
        InvocationSource source = InvocationSource::TrustedGui;
        QString clientId;
    };

    struct GuiCommandContext {
        WindowId windowId;
        bool validateOnly = false;
        InvocationSource source = InvocationSource::TrustedGui;
        QString clientId;
    };

    struct GuiDocumentCommandContext {
        DocumentVersion expected;
        WindowId windowId;
        bool validateOnly = false;
        InvocationSource source = InvocationSource::TrustedGui;
        QString clientId;
    };

    struct ApplicationCommandContext {
        bool validateOnly = false;
        InvocationSource source = InvocationSource::TrustedGui;
        QString clientId;
    };

    enum class ObjectKind {
        Unknown,
        Track,
        Clip,
        Note,
        InferPiece,
        Curve,
        Anchor,
        SpeakerMixKeyframe,
    };

    struct ObjectRef {
        ObjectKind kind = ObjectKind::Unknown;
        int value = -1;

        friend bool operator==(const ObjectRef &, const ObjectRef &) = default;
    };

    struct CreatedObjectRef {
        QString clientRef;
        ObjectRef object;

        friend bool operator==(const CreatedObjectRef &, const CreatedObjectRef &) = default;
    };

    enum class AutomationErrorCode {
        InvalidArgument,
        NotFound,
        WrongObjectType,
        DocumentChanged,
        RevisionConflict,
        IdempotencyConflict,
        OperationUnavailable,
        HostCapabilityUnavailable,
        ModuleNotReady,
        Busy,
        OperationNotCancelable,
        PathRequired,
        FileNotFound,
        FormatUnsupported,
        OverwriteDenied,
        IoError,
        InferenceError,
        PermissionDenied,
        TooManyRequests,
        Unsupported,
        InternalError,
    };

    QString errorCodeName(AutomationErrorCode code);

    struct AutomationError {
        AutomationErrorCode code = AutomationErrorCode::InternalError;
        QString message;
        OperationId operationId;
        QString fieldPath;
        std::optional<ObjectRef> object;
        std::optional<TaskId> taskId;
        std::optional<DocumentId> documentId;
        std::optional<Revision> expectedRevision;
        std::optional<Revision> actualRevision;

        [[nodiscard]] static AutomationError invalidArgument(QString fieldPath, QString message);
        [[nodiscard]] static AutomationError notFound(ObjectRef object, QString message);
        [[nodiscard]] static AutomationError wrongObjectType(ObjectRef object, QString message);
        [[nodiscard]] static AutomationError taskNotFound(TaskId taskId);
        [[nodiscard]] static AutomationError documentBusy(DocumentId documentId);
        [[nodiscard]] static AutomationError documentChanged(DocumentId requested,
                                                             DocumentId current);
        [[nodiscard]] static AutomationError revisionConflict(DocumentId documentId,
                                                              Revision expected, Revision actual);

        friend bool operator==(const AutomationError &, const AutomationError &) = default;
    };

    template <typename T>
    using AutomationResult = Expected<T, AutomationError>;

    // Rebased task results still require an immutable target snapshot check at the write boundary.
    [[nodiscard]] AutomationResult<DocumentVersion>
        rebaseTaskVersionWithinGeneration(const DocumentVersion &taskVersion,
                                          const DocumentVersion &currentVersion);

    struct AutomationUnit {
        friend bool operator==(const AutomationUnit &, const AutomationUnit &) = default;
    };

    struct MutationResult {
        DocumentVersion previous;
        DocumentVersion current;
        QList<ObjectRef> affectedObjects;
        QList<CreatedObjectRef> createdObjects;
        QStringList warnings;
        bool changed = false;
        bool validatedOnly = false;

        friend bool operator==(const MutationResult &, const MutationResult &) = default;
    };

    struct GuiMutationResult {
        WindowId windowId;
        bool changed = false;
        bool validatedOnly = false;

        friend bool operator==(const GuiMutationResult &, const GuiMutationResult &) = default;
    };

    struct ApplicationMutationResult {
        bool changed = false;
        bool validatedOnly = false;

        friend bool operator==(const ApplicationMutationResult &,
                               const ApplicationMutationResult &) = default;
    };

} // namespace Automation

Q_DECLARE_METATYPE(Automation::DocumentId)
Q_DECLARE_METATYPE(Automation::WindowId)
Q_DECLARE_METATYPE(Automation::TaskId)
Q_DECLARE_METATYPE(Automation::DocumentVersion)

#endif // AUTOMATIONTYPES_H
