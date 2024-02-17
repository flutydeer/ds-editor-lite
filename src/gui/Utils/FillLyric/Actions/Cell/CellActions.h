#ifndef DS_EDITOR_LITE_CELLACTIONS_H
#define DS_EDITOR_LITE_CELLACTIONS_H

#include <QModelIndex>

#include "../../Model/PhonicModel.h"
#include "../../History/MActionSequence.h"

namespace FillLyric {

    class CellActions : public MActionSequence {
    public:
        void cellClear(const QModelIndexList &indexes, PhonicModel *model);
    };

} // FillLyric

#endif // DS_EDITOR_LITE_CELLACTIONS_H
