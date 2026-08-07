#ifndef DS_EDITOR_LITE_IPROJECTFORMATHANDLER_H
#define DS_EDITOR_LITE_IPROJECTFORMATHANDLER_H

#include "Controller/DocumentWorkflow/ProjectLoadTypes.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

class IProjectLoadSession;

struct ProjectFormatDescriptor {
    QString id;
    QString displayName;
    QStringList extensions;
    bool canOpen = false;
    bool canImport = false;
    bool canExport = false;
};

struct ProjectLoadRequest {
    QString filePath;
    ProjectLoadPurpose purpose = ProjectLoadPurpose::Open;
    quint64 requestId = 0;
};

// A project file format backend: describes itself, probes file headers, and
// creates load sessions. Handlers must not touch the global AppModel, History
// or document paths directly - sessions only produce PreparedProject payloads.
class IProjectFormatHandler {
public:
    virtual ~IProjectFormatHandler() = default;

    virtual ProjectFormatDescriptor descriptor() const = 0;
    virtual bool probe(const QByteArray &header) const = 0;
    virtual IProjectLoadSession *createSession(const ProjectLoadRequest &request,
                                               QObject *parent) = 0;
};

#endif // DS_EDITOR_LITE_IPROJECTFORMATHANDLER_H
