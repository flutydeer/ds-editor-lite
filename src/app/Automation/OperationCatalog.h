#ifndef OPERATIONCATALOG_H
#define OPERATIONCATALOG_H

#include "AutomationTypes.h"

#include <QList>

namespace Automation {

    enum class OperationKind {
        Query,
        Command,
    };

    enum class SyncMode {
        Synchronous,
        Asynchronous,
    };

    enum class DocumentPolicy {
        None,
        Read,
        Write,
        Replace,
    };

    enum class RevisionPolicy {
        None,
        Check,
        Increment,
        Reset,
    };

    enum class HistoryPolicy {
        None,
        Record,
        UndoRedo,
    };

    enum class FileAccessPolicy {
        None,
        Read,
        Write,
        ReadWrite,
    };

    enum class HostAvailability {
        Core,
        GuiOnly,
    };

    enum class SafetyClass {
        ReadOnly,
        Reversible,
        FileSystem,
        Destructive,
        OpenWorld,
    };

    enum class ExposurePolicy {
        InternalOnly,
        AutomationCandidate,
    };

    enum class IdempotencyPolicy {
        Unsupported,
        DocumentGeneration,
    };

    struct OperationDescriptor {
        OperationId id;
        QString category;
        OperationKind kind = OperationKind::Query;
        SyncMode syncMode = SyncMode::Synchronous;
        DocumentPolicy documentPolicy = DocumentPolicy::None;
        RevisionPolicy revisionPolicy = RevisionPolicy::None;
        HistoryPolicy historyPolicy = HistoryPolicy::None;
        FileAccessPolicy fileAccess = FileAccessPolicy::None;
        HostAvailability hostAvailability = HostAvailability::Core;
        SafetyClass safety = SafetyClass::ReadOnly;
        ExposurePolicy exposure = ExposurePolicy::InternalOnly;
        IdempotencyPolicy idempotency = IdempotencyPolicy::Unsupported;
    };

    class OperationCatalog final {
    public:
        AutomationResult<AutomationUnit> add(OperationDescriptor descriptor);

        [[nodiscard]] const QList<OperationDescriptor> &entries() const;
        [[nodiscard]] const OperationDescriptor *find(const OperationId &id) const;
        [[nodiscard]] QStringList operationIds() const;

    private:
        QList<OperationDescriptor> m_entries;
    };

} // namespace Automation

#endif // OPERATIONCATALOG_H
