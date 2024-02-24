#include "CellCommon.h"

namespace FillLyric {
    void moveExecute(const QList<moveInfo> &moveList, PhonicModel *model) {
        for (const auto &move : moveList) {
            model->moveData(move.srcRow, move.srcCol, move.tarRow, move.tarCol);
        }
    }

    void moveUndo(const QList<moveInfo> &moveList, PhonicModel *model) {
        for (int i = static_cast<int>(moveList.size()) - 1; i >= 0; i--) {
            const auto move = moveList[i];
            model->moveData(move.tarRow, move.tarCol, move.srcRow, move.srcCol);
        }
    }
}
