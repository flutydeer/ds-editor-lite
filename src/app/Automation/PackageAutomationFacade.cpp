#include "PackageAutomationFacade.h"
#include "OperationIds.h"

#include <algorithm>

namespace Automation {
    namespace {
        AutomationError unavailable() {
            AutomationError error;
            error.code = AutomationErrorCode::ModuleNotReady;
            error.message = QStringLiteral("Package services are unavailable");
            return error;
        }

        AutomationError packageNotFound(const QString &packageId) {
            AutomationError error;
            error.code = AutomationErrorCode::NotFound;
            error.fieldPath = QStringLiteral("package_id");
            error.message = QStringLiteral("Package was not found: %1").arg(packageId);
            return error;
        }

        void projectPackagePath(PackageDto &package, const PackagePathProjection &projection) {
            if (!projection)
                return;
            if (const auto projected = projection(package.path))
                package.path = *projected;
            else
                package.path.clear();
        }

        void projectRefreshPaths(PackageRefreshResultDto &result,
                                 const PackagePathProjection &projection) {
            if (!projection)
                return;
            for (auto &failure : result.failures) {
                if (const auto projected = projection(failure.path))
                    failure.path = *projected;
                else
                    failure.path.clear();
            }
        }
    }

    PackageAutomationFacade::PackageAutomationFacade(OperationCatalog &catalog,
                                                     AutomationDispatcher &dispatcher,
                                                     PackageRuntimeServices services)
        : m_catalog(catalog), m_dispatcher(dispatcher), m_services(std::move(services)) {
        registerOperations();
    }

    AutomationResult<QList<PackageDto>> PackageAutomationFacade::getInstalledPackages() {
        return getInstalledPackages({});
    }

    AutomationResult<QList<PackageDto>>
        PackageAutomationFacade::getInstalledPackages(PackagePathProjection pathProjection) {
        return m_dispatcher.dispatchApplicationQuery<QList<PackageDto>>(
            OperationIds::packages::list, [this, pathProjection = std::move(pathProjection)] {
                if (!m_services.installedPackages)
                    return AutomationResult<QList<PackageDto>>(unavailable());
                auto packages = m_services.installedPackages();
                for (auto &package : packages)
                    projectPackagePath(package, pathProjection);
                return AutomationResult<QList<PackageDto>>(std::move(packages));
            });
    }

    AutomationResult<PackageDto>
        PackageAutomationFacade::describePackage(const QString &packageId,
                                                 PackagePathProjection pathProjection) {
        return describePackage(packageId, {}, std::move(pathProjection));
    }

    AutomationResult<PackageDto>
        PackageAutomationFacade::describePackage(const QString &packageId, const QString &version,
                                                 PackagePathProjection pathProjection) {
        return m_dispatcher.dispatchApplicationQuery<PackageDto>(
            OperationIds::packages::list,
            [this, packageId = packageId.trimmed(), version = version.trimmed(),
             pathProjection = std::move(pathProjection)]() mutable {
                if (packageId.isEmpty()) {
                    return AutomationResult<PackageDto>(AutomationError::invalidArgument(
                        QStringLiteral("package_id"), QStringLiteral("Package ID is empty")));
                }
                if (!m_services.installedPackages)
                    return AutomationResult<PackageDto>(unavailable());
                QVersionNumber requestedVersion;
                if (!version.isEmpty()) {
                    qsizetype suffixIndex = 0;
                    requestedVersion = QVersionNumber::fromString(version, &suffixIndex);
                    if (requestedVersion.isNull() || suffixIndex != version.size()) {
                        return AutomationResult<PackageDto>(AutomationError::invalidArgument(
                            QStringLiteral("version"),
                            QStringLiteral("Package version is invalid")));
                    }
                }
                const auto packages = m_services.installedPackages();
                const PackageDto *selected = nullptr;
                for (const auto &package : packages) {
                    if (package.id != packageId ||
                        (!version.isEmpty() && package.version != requestedVersion))
                        continue;
                    if (!selected ||
                        QVersionNumber::compare(package.version, selected->version) > 0)
                        selected = &package;
                }
                if (!selected)
                    return AutomationResult<PackageDto>(packageNotFound(packageId));
                auto result = *selected;
                projectPackagePath(result, pathProjection);
                return AutomationResult<PackageDto>(std::move(result));
            });
    }

    AutomationResult<AutomationUnit> PackageAutomationFacade::refreshPackages(
        const ApplicationCommandContext &context, PackageRefreshCompletion completion,
        PackagePathProjection pathProjection, PackageRefreshCommitGate commitGate) {
        return m_dispatcher.dispatchApplicationCommand<AutomationUnit>(
            OperationIds::packages::refresh, context,
            [this, completion = std::move(completion), pathProjection = std::move(pathProjection),
             commitGate = std::move(commitGate)](const bool validateOnly) mutable {
                if (!completion) {
                    return AutomationResult<AutomationUnit>(AutomationError::invalidArgument(
                        QStringLiteral("completion"),
                        QStringLiteral("Package refresh completion callback is missing")));
                }
                if (!m_services.installedPackages)
                    return AutomationResult<AutomationUnit>(unavailable());
                if (validateOnly) {
                    completion(PackageRefreshResultDto{
                        .packages = static_cast<int>(m_services.installedPackages().size()),
                    });
                    return AutomationResult<AutomationUnit>(AutomationUnit{});
                }
                if (!m_services.refreshPackages)
                    return AutomationResult<AutomationUnit>(unavailable());
                return m_services.refreshPackages(
                    std::move(commitGate),
                    [completion = std::move(completion),
                     pathProjection = std::move(pathProjection)](
                        AutomationResult<PackageRefreshResultDto> result) mutable {
                        if (result) {
                            auto projected = result.get();
                            projectRefreshPaths(projected, pathProjection);
                            completion(std::move(projected));
                        } else {
                            completion(result.getError());
                        }
                    });
            });
    }

    AutomationResult<PackageValidationReportDto>
        PackageAutomationFacade::validatePackage(const QString &path) {
        return m_dispatcher.dispatchApplicationQuery<PackageValidationReportDto>(
            OperationIds::packages::validate, [this, path] {
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
            OperationIds::packages::resolve_document_voices, context, {},
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
            .id = OperationIds::packages::list,
            .category = QStringLiteral("packages"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
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
            .id = OperationIds::packages::resolve_document_voices,
            .category = QStringLiteral("packages"),
            .kind = OperationKind::Command,
            .syncMode = SyncMode::Synchronous,
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
            .id = OperationIds::packages::refresh,
            .category = QStringLiteral("packages"),
            .kind = OperationKind::Command,
            .syncMode = SyncMode::Asynchronous,
            .documentPolicy = DocumentPolicy::None,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::Read,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::FileSystem,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
        add({
            .id = OperationIds::packages::validate,
            .category = QStringLiteral("packages"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
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
