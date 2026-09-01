#ifndef AUTOMATIONFILEGUARD_H
#define AUTOMATIONFILEGUARD_H

#include "../AutomationTypes.h"

#include <QReadWriteLock>
#include <QStringList>

namespace Automation {

    enum class FileAccessPurpose {
        Read,
        Write,
    };

    struct AuthorizedPath {
        QString canonicalPath;
        FileAccessPurpose purpose = FileAccessPurpose::Read;

        friend bool operator==(const AuthorizedPath &, const AuthorizedPath &) = default;
    };

    struct FileAccessSnapshot {
        QStringList accessRoots;
        QStringList sessionReadGrants;
        QStringList sessionWriteGrants;

        friend bool operator==(const FileAccessSnapshot &, const FileAccessSnapshot &) = default;
    };

    class AutomationFileGuard final {
    public:
        AutomationResult<AutomationUnit> setConfiguredRoots(const QStringList &accessRoots);
        AutomationResult<AuthorizedPath> addSessionGrant(const QString &path,
                                                         FileAccessPurpose purpose);
        void clearSessionGrants();

        [[nodiscard]] AutomationResult<AuthorizedPath> authorize(const QString &path,
                                                                 FileAccessPurpose purpose) const;
        [[nodiscard]] AutomationResult<AuthorizedPath>
            reauthorize(const AuthorizedPath &authorizedPath) const;
        [[nodiscard]] FileAccessSnapshot snapshot() const;

    private:
        struct PathRule {
            QString canonicalPath;
            bool directory = false;
        };

        static AutomationResult<PathRule> normalizeRoot(const QString &path);
        static AutomationResult<PathRule> normalizeGrant(const QString &path,
                                                         FileAccessPurpose purpose);
        static AutomationResult<QString> normalizeTarget(const QString &path,
                                                         FileAccessPurpose purpose);
        static bool matchesRule(const QString &canonicalPath, const PathRule &rule);
        static QStringList rulePaths(const QList<PathRule> &rules);

        mutable QReadWriteLock m_lock;
        QList<PathRule> m_accessRoots;
        QList<PathRule> m_sessionReadGrants;
        QList<PathRule> m_sessionWriteGrants;
    };

} // namespace Automation

#endif // AUTOMATIONFILEGUARD_H
