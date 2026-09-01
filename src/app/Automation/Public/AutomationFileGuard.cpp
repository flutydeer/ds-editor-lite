#include "AutomationFileGuard.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>
#ifdef Q_OS_WIN
#  include <filesystem>
#endif

namespace Automation {
    namespace {
        AutomationError pathError(const AutomationErrorCode code, QString message) {
            AutomationError error;
            error.code = code;
            error.fieldPath = QStringLiteral("path");
            error.message = std::move(message);
            return error;
        }

        QString normalizedSeparators(const QString &path) {
            return QDir::cleanPath(QDir::fromNativeSeparators(path));
        }

        bool sameCanonicalPath(const QString &left, const QString &right) {
            if (left == right)
                return true;
#ifdef Q_OS_WIN
            std::error_code error;
            const auto equivalent = std::filesystem::equivalent(
                std::filesystem::path(left.toStdWString()),
                std::filesystem::path(right.toStdWString()), error);
            return !error && equivalent;
#else
            return false;
#endif
        }

#ifdef Q_OS_WIN
        bool isWithinDirectory(const QString &canonicalPath, const QString &canonicalDirectory) {
            auto currentPath = canonicalPath;
            for (;;) {
                const QFileInfo current(currentPath);
                if (current.exists() && sameCanonicalPath(currentPath, canonicalDirectory))
                    return true;
                const auto parent = normalizedSeparators(current.dir().absolutePath());
                if (parent == currentPath)
                    return false;
                currentPath = parent;
            }
        }
#endif

        bool containsInvalidWindowsComponent(const QString &path) {
#ifdef Q_OS_WIN
            static const QRegularExpression reserved(
                QStringLiteral("^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\\..*)?$"),
                QRegularExpression::CaseInsensitiveOption);
            const auto components = normalizedSeparators(path).split(u'/', Qt::SkipEmptyParts);
            for (qsizetype index = 0; index < components.size(); ++index) {
                const auto &component = components.at(index);
                if (index == 0 && component.endsWith(u':'))
                    continue;
                if (component.endsWith(u' ') || component.endsWith(u'.') ||
                    reserved.match(component).hasMatch()) {
                    return true;
                }
            }
#else
            Q_UNUSED(path)
#endif
            return false;
        }

        template <typename Rule>
        bool appendUniqueRule(QList<Rule> &rules, Rule rule) {
            const auto duplicate =
                std::find_if(rules.cbegin(), rules.cend(), [&](const auto &current) {
                    return current.directory == rule.directory &&
                           sameCanonicalPath(current.canonicalPath, rule.canonicalPath);
                });
            if (duplicate != rules.cend())
                return false;
            rules.append(std::move(rule));
            return true;
        }
    }

    AutomationResult<AutomationUnit>
        AutomationFileGuard::setConfiguredRoots(const QStringList &accessRoots) {
        QList<PathRule> normalizedAccessRoots;
        for (const auto &path : accessRoots) {
            auto normalized = normalizeRoot(path);
            if (!normalized)
                return normalized.getError();
            appendUniqueRule(normalizedAccessRoots, normalized.get());
        }

        const QWriteLocker locker(&m_lock);
        m_accessRoots = std::move(normalizedAccessRoots);
        return AutomationUnit{};
    }

    AutomationResult<AuthorizedPath>
        AutomationFileGuard::addSessionGrant(const QString &path, const FileAccessPurpose purpose) {
        auto normalized = normalizeGrant(path, purpose);
        if (!normalized)
            return normalized.getError();

        const QWriteLocker locker(&m_lock);
        auto &rules =
            purpose == FileAccessPurpose::Read ? m_sessionReadGrants : m_sessionWriteGrants;
        appendUniqueRule(rules, normalized.get());
        return AuthorizedPath{normalized.get().canonicalPath, purpose};
    }

    void AutomationFileGuard::clearSessionGrants() {
        const QWriteLocker locker(&m_lock);
        m_sessionReadGrants.clear();
        m_sessionWriteGrants.clear();
    }

    AutomationResult<AuthorizedPath>
        AutomationFileGuard::authorize(const QString &path, const FileAccessPurpose purpose) const {
        auto normalized = normalizeTarget(path, purpose);
        if (!normalized)
            return normalized.getError();

        const QReadLocker locker(&m_lock);
        const auto &grants =
            purpose == FileAccessPurpose::Read ? m_sessionReadGrants : m_sessionWriteGrants;
        const auto allowedBy = [&](const QList<PathRule> &rules) {
            return std::any_of(rules.cbegin(), rules.cend(), [&](const auto &rule) {
                return matchesRule(normalized.get(), rule);
            });
        };
        if (!allowedBy(m_accessRoots) && !allowedBy(grants)) {
            return pathError(
                AutomationErrorCode::PermissionDenied,
                QStringLiteral("Path is outside the configured automation access roots"));
        }
        return AuthorizedPath{normalized.get(), purpose};
    }

    AutomationResult<AuthorizedPath>
        AutomationFileGuard::reauthorize(const AuthorizedPath &authorizedPath) const {
        auto current = authorize(authorizedPath.canonicalPath, authorizedPath.purpose);
        if (!current)
            return current.getError();
        if (!sameCanonicalPath(current.get().canonicalPath, authorizedPath.canonicalPath)) {
            return pathError(AutomationErrorCode::PermissionDenied,
                             QStringLiteral("Path changed after it was authorized"));
        }
        return current;
    }

    FileAccessSnapshot AutomationFileGuard::snapshot() const {
        const QReadLocker locker(&m_lock);
        return {
            .accessRoots = rulePaths(m_accessRoots),
            .sessionReadGrants = rulePaths(m_sessionReadGrants),
            .sessionWriteGrants = rulePaths(m_sessionWriteGrants),
        };
    }

    AutomationResult<AutomationFileGuard::PathRule>
        AutomationFileGuard::normalizeRoot(const QString &path) {
        auto normalized = normalizeTarget(path, FileAccessPurpose::Read);
        if (!normalized)
            return normalized.getError();
        const QFileInfo info(normalized.get());
        if (!info.isDir()) {
            return pathError(
                AutomationErrorCode::InvalidArgument,
                QStringLiteral("Automation access root must be an existing directory"));
        }
        return PathRule{normalized.get(), true};
    }

    AutomationResult<AutomationFileGuard::PathRule>
        AutomationFileGuard::normalizeGrant(const QString &path, const FileAccessPurpose purpose) {
        auto normalized = normalizeTarget(path, purpose);
        if (!normalized)
            return normalized.getError();
        const QFileInfo info(normalized.get());
        return PathRule{normalized.get(), info.exists() && info.isDir()};
    }

    AutomationResult<QString>
        AutomationFileGuard::normalizeTarget(const QString &path, const FileAccessPurpose purpose) {
        if (path.isEmpty() || path.contains(QChar::Null) || !QDir::isAbsolutePath(path) ||
            containsInvalidWindowsComponent(path)) {
            return pathError(AutomationErrorCode::InvalidArgument,
                             QStringLiteral("Path must be an absolute, valid platform path"));
        }

        const auto cleaned = normalizedSeparators(path);
        QFileInfo target(cleaned);
        if (target.exists()) {
            const auto canonical = normalizedSeparators(target.canonicalFilePath());
            if (canonical.isEmpty()) {
                return pathError(AutomationErrorCode::IoError,
                                 QStringLiteral("Path could not be canonicalized"));
            }
            return canonical;
        }
        if (purpose == FileAccessPurpose::Read) {
            return pathError(AutomationErrorCode::FileNotFound,
                             QStringLiteral("Readable path does not exist"));
        }

        QStringList missingComponents;
        auto currentPath = cleaned;
        for (;;) {
            const QFileInfo current(currentPath);
            if (current.exists()) {
                if (!current.isDir()) {
                    return pathError(AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("Output path has a non-directory parent"));
                }
                auto canonical = normalizedSeparators(current.canonicalFilePath());
                if (canonical.isEmpty()) {
                    return pathError(AutomationErrorCode::IoError,
                                     QStringLiteral("Output parent could not be canonicalized"));
                }
                for (const auto &component : missingComponents)
                    canonical = normalizedSeparators(QDir(canonical).filePath(component));
                return canonical;
            }

            const auto component = current.fileName();
            const auto parent = normalizedSeparators(current.dir().absolutePath());
            if (component.isEmpty() || parent == currentPath) {
                return pathError(AutomationErrorCode::IoError,
                                 QStringLiteral("Output path has no existing canonical parent"));
            }
            missingComponents.prepend(component);
            currentPath = parent;
        }
    }

    bool AutomationFileGuard::matchesRule(const QString &canonicalPath, const PathRule &rule) {
        if (sameCanonicalPath(canonicalPath, rule.canonicalPath))
            return true;
        if (!rule.directory)
            return false;
        const auto prefix =
            rule.canonicalPath.endsWith(u'/') ? rule.canonicalPath : rule.canonicalPath + u'/';
        if (canonicalPath.startsWith(prefix, Qt::CaseSensitive))
            return true;
#ifdef Q_OS_WIN
        return isWithinDirectory(canonicalPath, rule.canonicalPath);
#else
        return false;
#endif
    }

    QStringList AutomationFileGuard::rulePaths(const QList<PathRule> &rules) {
        QStringList result;
        result.reserve(rules.size());
        for (const auto &rule : rules)
            result.append(rule.canonicalPath);
        return result;
    }

} // namespace Automation
