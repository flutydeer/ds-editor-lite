#include "AutomationTypes.h"

namespace Automation {

    QString errorCodeName(const AutomationErrorCode code) {
        switch (code) {
            case AutomationErrorCode::InvalidArgument:
                return QStringLiteral("invalid_argument");
            case AutomationErrorCode::NotFound:
                return QStringLiteral("not_found");
            case AutomationErrorCode::WrongObjectType:
                return QStringLiteral("wrong_object_type");
            case AutomationErrorCode::DocumentChanged:
                return QStringLiteral("document_changed");
            case AutomationErrorCode::RevisionConflict:
                return QStringLiteral("revision_conflict");
            case AutomationErrorCode::IdempotencyConflict:
                return QStringLiteral("idempotency_conflict");
            case AutomationErrorCode::OperationUnavailable:
                return QStringLiteral("operation_unavailable");
            case AutomationErrorCode::HostCapabilityUnavailable:
                return QStringLiteral("host_capability_unavailable");
            case AutomationErrorCode::ModuleNotReady:
                return QStringLiteral("module_not_ready");
            case AutomationErrorCode::Busy:
                return QStringLiteral("busy");
            case AutomationErrorCode::OperationNotCancelable:
                return QStringLiteral("operation_not_cancelable");
            case AutomationErrorCode::IoError:
                return QStringLiteral("io_error");
            case AutomationErrorCode::InternalError:
                return QStringLiteral("internal_error");
        }
        return QStringLiteral("internal_error");
    }

    AutomationError AutomationError::invalidArgument(QString fieldPath, QString message) {
        AutomationError error;
        error.code = AutomationErrorCode::InvalidArgument;
        error.fieldPath = std::move(fieldPath);
        error.message = std::move(message);
        return error;
    }

    AutomationError AutomationError::notFound(const ObjectRef object, QString message) {
        AutomationError error;
        error.code = AutomationErrorCode::NotFound;
        error.object = object;
        error.message = std::move(message);
        return error;
    }

    AutomationError AutomationError::wrongObjectType(const ObjectRef object, QString message) {
        AutomationError error;
        error.code = AutomationErrorCode::WrongObjectType;
        error.object = object;
        error.message = std::move(message);
        return error;
    }

    AutomationError AutomationError::taskNotFound(TaskId taskId) {
        AutomationError error;
        error.code = AutomationErrorCode::NotFound;
        error.taskId = std::move(taskId);
        error.message = QStringLiteral("Automation task was not found");
        return error;
    }

    AutomationError AutomationError::documentBusy(DocumentId documentId) {
        AutomationError error;
        error.code = AutomationErrorCode::Busy;
        error.documentId = std::move(documentId);
        error.message = QStringLiteral("Document is changing and cannot accept this operation");
        return error;
    }

    AutomationError AutomationError::documentChanged(const DocumentId requested,
                                                       const DocumentId current) {
        AutomationError error;
        error.code = AutomationErrorCode::DocumentChanged;
        error.documentId = requested;
        error.message = QStringLiteral("Document %1 is no longer active; current document is %2")
                            .arg(requested.toString(), current.toString());
        return error;
    }

    AutomationError AutomationError::revisionConflict(const DocumentId documentId,
                                                       const Revision expected,
                                                       const Revision actual) {
        AutomationError error;
        error.code = AutomationErrorCode::RevisionConflict;
        error.documentId = documentId;
        error.expectedRevision = expected;
        error.actualRevision = actual;
        error.message = QStringLiteral("Expected revision %1, actual revision is %2")
                            .arg(expected)
                            .arg(actual);
        return error;
    }

} // namespace Automation
