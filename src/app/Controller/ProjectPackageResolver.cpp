#include "ProjectPackageResolver.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include "Model/AppStatus/AppStatus.h"
#include <lite/PackageManager/PackageManager.h>

#include <QDebug>
#include <QTimer>

ProjectPackageResolver::ProjectPackageResolver(QObject *parent) : QObject(parent) {
    connect(appModel, &AppModel::modelChanged, this, &ProjectPackageResolver::scheduleResolve);
    connect(packageManager, &PackageManager::packagesRefreshed, this,
            [this](const QList<PackageInfo> &) { scheduleResolve(); });
    connect(appStatus, &AppStatus::moduleStatusChanged, this,
            [this](const AppStatus::ModuleType module, const AppStatus::ModuleStatus status) {
                if (module == AppStatus::ModuleType::Package &&
                    status == AppStatus::ModuleStatus::Ready)
                    scheduleResolve();
            });
}

ProjectPackageResolver::~ProjectPackageResolver() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(ProjectPackageResolver)

void ProjectPackageResolver::scheduleResolve() {
    if (m_resolveScheduled)
        return;

    m_resolveScheduled = true;
    QTimer::singleShot(0, this, [this] {
        m_resolveScheduled = false;
        resolveProject();
    });
}

void ProjectPackageResolver::resolveProject() {
    if (appStatus->packageModuleStatus != AppStatus::ModuleStatus::Ready)
        return;
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return;
    const auto result = runtime->packages().resolveDocumentVoices({
        .expected = runtime->documentVersion(),
        .source = Automation::InvocationSource::TrustedGui,
    });
    if (result && result.get().changed)
        qInfo() << "Resolved project singer/speaker metadata from installed packages";
}
