#ifndef CLIPRESIZEUTILS_H
#define CLIPRESIZEUTILS_H

#include <lite/ProjectModel/AppModel/Clip.h>

namespace ClipResizeUtils {
    bool updateRightEdge(Clip::ClipCommonProperties &properties, int requestedClipLength,
                         bool lengthResizable, int contentLength);
}

#endif // CLIPRESIZEUTILS_H
