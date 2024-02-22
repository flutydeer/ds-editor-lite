#ifndef DS_EDITOR_LITE_WRAPCELLACTIONS_H
#define DS_EDITOR_LITE_WRAPCELLACTIONS_H

#include <QModelIndex>
#include "../../History/MActionSequence.h"

#include "DeleteWrapCellAction.h"
#include "InsertWarpCellAction.h"

namespace FillLyric {

    class WrapCellActions : public MActionSequence {
    public:
        void deleteWrapCell(const QModelIndex &index, PhonicModel *model);
        void insertWrapCell(const QModelIndex &index, PhonicModel *model);
    };

} // FillLyric

#endif // DS_EDITOR_LITE_WRAPCELLACTIONS_H
