#include "PianoRollCoord.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <algorithm>

double PianoRollCoord::keyIndexToSceneY(const double index, const double keyHeight) {
    return (127 - index) * keyHeight;
}

double PianoRollCoord::sceneYToKeyIndexDouble(const double y, const double keyHeight) {
    return 127 - y / keyHeight;
}

int PianoRollCoord::sceneYToKeyIndexInt(const double y, const double keyHeight) {
    const auto keyIndexD = 127 - y / keyHeight;
    auto keyIndex = static_cast<int>(keyIndexD) + 1;
    if (keyIndex > 127)
        keyIndex = 127;
    return keyIndex;
}