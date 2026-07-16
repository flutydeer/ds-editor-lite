#ifndef COMPUTEAUDIOHASHTASK_H
#define COMPUTEAUDIOHASHTASK_H

#include "Modules/Task/Task.h"

// Computes the SHA-512 digest (lowercase hex) of an audio file in the background,
// used for project portability verification, see Utils/DiffscopeAudioWorkspace.h
class ComputeAudioHashTask : public Task {
public:
    int clipId = -1;
    QString path;
    QString resultSha512;
    bool success = false;

private:
    void runTask() override;
};

#endif // COMPUTEAUDIOHASHTASK_H
