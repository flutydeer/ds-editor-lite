#ifndef COMPUTEAUDIOHASHTASK_H
#define COMPUTEAUDIOHASHTASK_H

#include "Automation/AutomationTypes.h"

#include <lite/Tasking/Task.h>

// Computes the SHA-512 digest (lowercase hex) of an audio file in the background,
// used for project portability verification, see Utils/DiffscopeAudioWorkspace.h
class ComputeAudioHashTask : public Task {
public:
    int clipId = -1;
    Automation::DocumentVersion documentVersion;
    Automation::TaskId automationTaskId;
    QString path;
    QString resultSha512;
    bool success = false;

private:
    void runTask() override;
};

#endif // COMPUTEAUDIOHASHTASK_H
