#include "DocumentWorkflowPathUtils.h"

#include <QDir>
#include <QFileInfo>

namespace DocumentWorkflowPathUtils {
    QString normalizedProjectPath(const QString &path) {
        return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    }

    bool projectPathsEqual(const QString &lhs, const QString &rhs) {
#ifdef Q_OS_WIN
        return QString::compare(lhs, rhs, Qt::CaseInsensitive) == 0;
#else
        return lhs == rhs;
#endif
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
