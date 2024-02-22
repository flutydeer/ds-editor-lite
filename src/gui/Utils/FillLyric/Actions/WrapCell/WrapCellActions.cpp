#include "WrapCellActions.h"

namespace FillLyric {
    void WrapCellActions::deleteWrapCell(const QModelIndex &index, PhonicModel *model) {
        addAction(DeleteWrapCellAction::build(index, model));
    }

    void WrapCellActions::insertWrapCell(const QModelIndex &index, PhonicModel *model) {
        addAction(InsertWarpCellAction::build(index, model));
    }
} // FillLyric