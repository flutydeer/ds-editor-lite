#ifndef RESOLVEAUDIOPATHTASK_H
#define RESOLVEAUDIOPATHTASK_H

#include "Modules/Task/Task.h"

// When the absolute path of an audio file is broken, relocates the file in the background, in candidate order:
// 1. project dir + relativeDir + fileName (sha512 must match)
// 2. project dir + fileName (sha512 must match)
// When expectedSha512 is empty (project saved by an editor that does not write the field),
// the first existing candidate is matched by file name and marked HitUnconfirmed for user confirmation
class ResolveAudioPathTask : public Task {
public:
    enum class Result { HitRelative, HitSibling, HitUnconfirmed, Miss };

    int clipId = -1;
    QString originalPath;
    QString relativeDir;
    QString fileName;
    QString expectedSha512;
    QString projectDir;

    Result result = Result::Miss;
    QString resolvedPath;

private:
    void runTask() override;
    QString computeSha512(const QString &filePath) const;
};

#endif // RESOLVEAUDIOPATHTASK_H
