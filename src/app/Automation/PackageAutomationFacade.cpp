#include "PackageAutomationFacade.h"

namespace Automation {
    namespace {
        AutomationError unavailable() {
            AutomationError error;
            error.code = AutomationErrorCode::ModuleNotReady;
            error.message = QStringLiteral("Package services are unavailable");
            return error;
        }
    }

    PackageAutomationFacade::PackageAutomationFacade(OperationCatalog &catalog,
                                                     AutomationDispatcher &dispatcher,
                                                     PackageRuntimeServices services)
        : m_catalog(catalog), m_dispatcher(dispatcher), m_services(std::move(services)) {
        registerOperations();
    }

    AutomationResult<QList<PackageDto>> PackageAutomationFacade::getInstalledPackages() {
        return m_dispatcher.dispatchApplicationQuery<QList<PackageDto>>(
            QStringLiteral("packages.list"), [this] {
                if (!m_services.installedPackages)
                    return AutomationResult<QList<PackageDto>>(unavailable());
                return AutomationResult<QList<PackageDto>>(m_services.installedPackages());
            });
    }

    AutomationResult<PackageValidationReportDto>
    PackageAutomationFacade::validatePackage(const QString &path) {
        return m_dispatcher.dispatchApplicationQuery<PackageValidationReportDto>(
            QStringLiteral("packages.validate"), [this, path] {
                if (path.trimmed().isEmpty()) {
                    return AutomationResult<PackageValidationReportDto>(
                        AutomationError::invalidArgument(QStringLiteral("path"),
                                                         QStringLiteral("Package path is empty")));
                }
                if (!m_services.validatePackage)
                    return AutomationResult<PackageValidationReportDto>(unavailable());
                return m_services.validatePackage(path);
            });
    }

    AutomationResult<MutationResult>
    PackageAutomationFacade::resolveDocumentVoices(const CommandContext &context) {
        return m_dispatcher.dispatchDocumentCommand(
            QStringLiteral("packages.resolve_document_voices"), context, {},
            [this](DocumentSession &session, const bool validateOnly) {
                if (!m_services.resolveDocumentVoices)
                    return AutomationResult<MutationResult>(unavailable());
                const auto previous = session.version();
                const int resolvedCount =
                    m_services.resolveDocumentVoices(session.model(), !validateOnly);
                return AutomationResult<MutationResult>({
                    .previous = previous,
                    .current = previous,
                    .changed = resolvedCount > 0,
                    .validatedOnly = validateOnly,
                });
            });
    }

    void PackageAutomationFacade::registerOperations() {
        const auto add = [this](OperationDescriptor descriptor) {
            const auto result = m_catalog.add(std::move(descriptor));
            Q_ASSERT(result);
        };
        add({
            .id = QStringLiteral("packages.list"),
            .category = QStringLiteral("packages"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .inputContract = QStringLiteral("automation.Empty.v1"),
            .outputContract = QStringLiteral("automation.PackageList.v1"),
            .documentPolicy = DocumentPolicy::None,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
        add({
            .id = QStringLiteral("packages.resolve_document_voices"),
            .category = QStringLiteral("packages"),
            .kind = OperationKind::Command,
            .syncMode = SyncMode::Synchronous,
            .inputContract = QStringLiteral("automation.DocumentCommand.v1"),
            .outputContract = QStringLiteral("automation.MutationResult.v1"),
            .documentPolicy = DocumentPolicy::Write,
            .revisionPolicy = RevisionPolicy::Check,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::Reversible,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
        add({
            .id = QStringLiteral("packages.validate"),
            .category = QStringLiteral("packages"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .inputContract = QStringLiteral("automation.PathQuery.v1"),
            .outputContract = QStringLiteral("automation.PackageValidationReport.v1"),
            .documentPolicy = DocumentPolicy::None,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::Read,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
    }

} // namespace Automation
