#ifndef IMPORTPROJECTACTIONS_H
#define IMPORTPROJECTACTIONS_H

#include "Model/AppModel/ProjectModelData.h"
#include "Modules/History/ActionSequence.h"

class AppModel;

class ImportProjectActions final : public ActionSequence {
public:
    ImportProjectActions(ProjectModelData &&data, bool importTempo, bool importTimeSignature,
                         AppModel *model);
};

#endif // IMPORTPROJECTACTIONS_H
