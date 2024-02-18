#ifndef DS_EDITOR_LITE_CELLCOMMON_H
#define DS_EDITOR_LITE_CELLCOMMON_H

#include <QList>
#include "../../Model/PhonicModel.h"

namespace FillLyric {
    struct moveInfo {
        int srcRow;
        int srcCol;
        int tarRow;
        int tarCol;
    };

    void moveExecute(const QList<moveInfo> &moveList, PhonicModel *model);
    void moveUndo(const QList<moveInfo> &moveList, PhonicModel *model);
}
#endif // DS_EDITOR_LITE_CELLCOMMON_H
