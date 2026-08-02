#include "ValidationUtils.h"

#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QDebug>

bool ValidationUtils::canInferDuration(const SingingClip &clip) {
    if (clip.notes().hasOverlappedItem())
        return false;
    for (const auto note : clip.notes()) {
        if (note->pronunciation().result().isEmpty()) {
            qCritical() << "Invalid note pronunciation";
            return false;
        }
        // TODO: 校验音素名称序列
    }
    return true;
}