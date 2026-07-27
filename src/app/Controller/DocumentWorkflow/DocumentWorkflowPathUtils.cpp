#include "DocumentWorkflowPathUtils.h"

#include <QDir>
#include <QFileInfo>

namespace DocumentWorkflowPathUtils {
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
