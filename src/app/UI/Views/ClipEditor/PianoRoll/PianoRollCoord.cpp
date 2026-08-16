#include "PianoRollCoord.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <algorithm>

double PianoRollCoord::keyIndexToSceneY(const double index, const double keyHeight) {
    return (127 - index) * keyHeight;
}

double PianoRollCoord::sceneYToKeyIndexDouble(const double y, const double keyHeight) {
    return 127 - y / keyHeight;
}

double PianoRollCoord::keyIndexToCenterY(const double index, const double keyHeight) {
    return keyIndexToSceneY(index, keyHeight) + keyHeight * 0.5;
}

double PianoRollCoord::centerYToKeyIndex(const double y, const double keyHeight) {
    return sceneYToKeyIndexDouble(y, keyHeight) + 0.5;
}

int PianoRollCoord::sceneYToKeyIndexInt(const double y, const double keyHeight) {
    const auto keyIndexD = 127 - y / keyHeight;
    auto keyIndex = static_cast<int>(keyIndexD) + 1;
    if (keyIndex > 127)
        keyIndex = 127;
    return keyIndex;
}
