#ifndef PACKAGEAUTOMATIONFACADE_H
#define PACKAGEAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"

#include <lite/ProjectModel/Voice/SingerInfo.h>

#include <QVersionNumber>
#include <QMap>

#include <functional>
#include <optional>

class AppModel;

namespace Automation {

    struct PackageSingerDto {
        QString singerId;
        QString packageId;
        QVersionNumber packageVersion;
        QString name;
        SingerInfo info;

        friend bool operator==(const PackageSingerDto &, const PackageSingerDto &) = default;
    };

    struct PackageDto {
        QString id;
        QVersionNumber version;
        QString vendor;
        QMap<QString, QString> localizedVendor;
        QString description;
        QMap<QString, QString> localizedDescription;
        QString license;
        QMap<QString, QString> localizedLicense;
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

    struct PackageRefreshFailureDto {
        QString path;
        QString reason;

        friend bool operator==(const PackageRefreshFailureDto &,
                               const PackageRefreshFailureDto &) = default;
    };

    struct PackageRefreshResultDto {
        int packages = 0;
        QStringList added;
        QStringList updated;
        QStringList removed;
        QList<PackageRefreshFailureDto> failures;

        friend bool operator==(const PackageRefreshResultDto &,
                               const PackageRefreshResultDto &) = default;
    };

    using PackagePathProjection =
        std::function<std::optional<QString>(const QString &canonicalPath)>;
    using PackageRefreshCommitGate = std::function<bool()>;
    using PackageRefreshCompletion = std::function<void(AutomationResult<PackageRefreshResultDto>)>;

    struct PackageRuntimeServices {
        std::function<QList<PackageDto>()> installedPackages;
        std::function<AutomationResult<PackageValidationReportDto>(const QString &)>
            validatePackage;
        std::function<int(AppModel *, bool apply)> resolveDocumentVoices;
        std::function<AutomationResult<AutomationUnit>(PackageRefreshCommitGate,
                                                       PackageRefreshCompletion)>
            refreshPackages;
    };

    class PackageAutomationFacade final {
    public:
        PackageAutomationFacade(AutomationDispatcher &dispatcher,
                                PackageRuntimeServices services = {});

        AutomationResult<QList<PackageDto>> getInstalledPackages();
        AutomationResult<QList<PackageDto>>
            getInstalledPackages(PackagePathProjection pathProjection);
        AutomationResult<PackageDto> describePackage(const QString &packageId,
                                                     PackagePathProjection pathProjection = {});
        AutomationResult<PackageDto> describePackage(const QString &packageId,
                                                     const QString &version,
                                                     PackagePathProjection pathProjection = {});
        AutomationResult<AutomationUnit> refreshPackages(const ApplicationCommandContext &context,
                                                         PackageRefreshCompletion completion,
                                                         PackagePathProjection pathProjection = {},
                                                         PackageRefreshCommitGate commitGate = {});
        AutomationResult<PackageValidationReportDto> validatePackage(const QString &path);
        AutomationResult<MutationResult> resolveDocumentVoices(const CommandContext &context);

    private:
        AutomationDispatcher &m_dispatcher;
        PackageRuntimeServices m_services;
    };

} // namespace Automation

#endif // PACKAGEAUTOMATIONFACADE_H
