#ifndef PIANOROLLCOORD_H
#define PIANOROLLCOORD_H

class PianoRollCoord {
public:
    PianoRollCoord() = delete;

    static double keyIndexToSceneY(double index, double keyHeight);
    static double sceneYToKeyIndexDouble(double y, double keyHeight);
    static int sceneYToKeyIndexInt(double y, double keyHeight);
};

#endif // PIANOROLLCOORD_H