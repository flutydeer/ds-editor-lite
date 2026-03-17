//
// Created by FlutyDeer on 2026/2/5.
//

#ifndef DS_EDITOR_LITE_PIECEUTILS_H
#define DS_EDITOR_LITE_PIECEUTILS_H

class InferPiece;
class Segment;

class PieceUtils {
public:
    // Check if existing piece and segment are the same
    // 1. Head padding length is the same
    // 2. Note sequence is the same
    // 3. Tail padding length is the same
    static bool isSamePiece(const InferPiece &left, const Segment &right);
};

#endif //DS_EDITOR_LITE_PIECEUTILS_H
