#include "WrapCellActions.h"

namespace FillLyric {
    void WrapCellActions::deleteWrapCell(const QModelIndex &index, PhonicModel *model,
                                         QTableView *tableView) {
        addAction(DeleteWrapCellAction::build(index, model, tableView));
    }

    void WrapCellActions::insertWrapCell(const QModelIndex &index, PhonicModel *model,
                                         QTableView *tableView) {
        addAction(InsertWarpCellAction::build(index, model, tableView));
    }
} // FillLyric