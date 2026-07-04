//
// Created by fluty on 24-9-27.
//

#include "ValidationUtils.h"

#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"

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