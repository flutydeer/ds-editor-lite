#ifndef DOCUMENTWORKFLOWPATHUTILS_H
#define DOCUMENTWORKFLOWPATHUTILS_H

#include <QString>

namespace DocumentWorkflowPathUtils {
    QString suggestedSavePath(const QString &projectPath, const QString &lastProjectFolder,
                              const QString &projectName);
}

#endif // DOCUMENTWORKFLOWPATHUTILS_H
