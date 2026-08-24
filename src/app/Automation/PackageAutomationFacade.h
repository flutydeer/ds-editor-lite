#ifndef PACKAGEAUTOMATIONFACADE_H
#define PACKAGEAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"

#include <QVersionNumber>

#include <functional>

class AppModel;

namespace Automation {

    struct PackageSingerDto {
        QString singerId;
        QString packageId;
        QVersionNumber packageVersion;
        QString name;

        friend bool operator==(const PackageSingerDto &, const PackageSingerDto &) = default;
    };

    struct PackageDto {
        QString id;
        QVersionNumber version;
        QString vendor;
        QString description;
        QString license;
        QString readme;
        QString url;
        QString path;
        QList<PackageSingerDto> singers;

        friend bool operator==(const PackageDto &, const PackageDto &) = default;
    };

    enum class PackageValidationSeverity {
        Info,
        Warning,
        Error,
    };

    struct PackageValidationItemDto {
        PackageValidationSeverity severity = PackageValidationSeverity::Info;
        QString path;
        QString message;
        QString actualValue;
        QString recommendation;

        friend bool operator==(const PackageValidationItemDto &,
                               const PackageValidationItemDto &) = default;
    };

    struct PackageValidationReportDto {
        QList<PackageValidationItemDto> items;
        bool hasErrors = false;

        friend bool operator==(const PackageValidationReportDto &,
                               const PackageValidationReportDto &) = default;
    };

    struct PackageRuntimeServices {
        std::function<QList<PackageDto>()> installedPackages;
        std::function<AutomationResult<PackageValidationReportDto>(const QString &)>
            validatePackage;
        std::function<int(AppModel *, bool apply)> resolveDocumentVoices;
    };

    class PackageAutomationFacade final {
    public:
        PackageAutomationFacade(OperationCatalog &catalog, AutomationDispatcher &dispatcher,
                                PackageRuntimeServices services = {});

        AutomationResult<QList<PackageDto>> getInstalledPackages();
        AutomationResult<PackageValidationReportDto> validatePackage(const QString &path);
        AutomationResult<MutationResult> resolveDocumentVoices(const CommandContext &context);

    private:
        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        PackageRuntimeServices m_services;
    };

} // namespace Automation

#endif // PACKAGEAUTOMATIONFACADE_H
