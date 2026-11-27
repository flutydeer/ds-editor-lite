#include "DocumentWorkflowPathUtils.h"

#include <QDir>
#include <QFileInfo>

namespace DocumentWorkflowPathUtils {
    QString normalizedProjectPath(const QString &path) {
        return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    }

    static QString projectIdentityPath(const QString &path) {
        const QFileInfo info(path);
        const auto canonical = info.canonicalFilePath();
        if (!canonical.isEmpty())
            return canonical;
        const auto directory = info.dir().canonicalPath();
        return directory.isEmpty() ? QDir::cleanPath(info.absoluteFilePath())
                                   : QDir(directory).filePath(info.fileName());
    }

    bool projectPathsEqual(const QString &lhs, const QString &rhs) {
#ifdef Q_OS_WIN
        constexpr auto sensitivity = Qt::CaseInsensitive;
#else
        constexpr auto sensitivity = Qt::CaseSensitive;
#endif
        if (QString::compare(lhs, rhs, sensitivity) == 0)
            return true;
        if (lhs.isEmpty() || rhs.isEmpty())
            return false;
        return QString::compare(projectIdentityPath(lhs), projectIdentityPath(rhs), sensitivity) ==
               0;
    }

    QString suggestedSavePath(const QString &projectPath, const QString &lastProjectFolder,
                              const QString &projectName) {
        if (!projectPath.isEmpty())
            return projectPath;

        auto fileName = projectName;
        if (QFileInfo(fileName).suffix().compare(QStringLiteral("dspx"), Qt::CaseInsensitive) != 0)
            fileName += QStringLiteral(".dspx");
        return QDir(lastProjectFolder).filePath(fileName);
    }
}
