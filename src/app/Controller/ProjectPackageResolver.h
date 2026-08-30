#ifndef PROJECTPACKAGERESOLVER_H
#define PROJECTPACKAGERESOLVER_H

#define projectPackageResolver ProjectPackageResolver::instance()

#include <lite/ProjectModel/Voice/SingerInfo.h>
#include <lite/ProjectModel/Voice/SpeakerInfo.h>
#include <lite/Core/Singleton.h>

#include <QObject>

class ProjectPackageResolver final : public QObject {
private:
    explicit ProjectPackageResolver(QObject *parent = nullptr);
    ~ProjectPackageResolver() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(ProjectPackageResolver)
    Q_DISABLE_COPY_MOVE(ProjectPackageResolver)

private:
    void scheduleResolve();
    void resolveProject();
    bool m_resolveScheduled = false;
};

#endif // PROJECTPACKAGERESOLVER_H
