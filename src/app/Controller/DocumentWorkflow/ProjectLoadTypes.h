#ifndef PROJECTLOADTYPES_H
#define PROJECTLOADTYPES_H

#include "Model/AppModel/LoopSettings.h"
#include "Model/AppModel/ProjectModelData.h"

#include <QString>

#include <variant>

enum class ProjectLoadPurpose { Open, Import };
enum class ProjectSourceKind { Native, Foreign };

struct ProjectLoadProgress {
    QString title;
    QString message;
    int minimum = 0;
    int maximum = 100;
    int value = 0;
    bool indeterminate = false;
};

struct ProjectOperationError {
    QString title;
    QString message;
};

struct ReplaceProjectPayload {
    ProjectModelData model;
    LoopSettings loopSettings;
    ProjectSourceKind sourceKind = ProjectSourceKind::Native;
    QString sourcePath;
    QString displayName;
};

struct AppendProjectPayload {
    ProjectModelData model;
    bool importTempo = false;
    bool importTimeSignature = false;
    QString sourcePath;
};

using PreparedProject = std::variant<std::monostate, ReplaceProjectPayload, AppendProjectPayload>;

#endif // PROJECTLOADTYPES_H
