#include "WrapCellActions.h"

namespace FillLyric {
    void WrapCellActions::deleteWrapCell(const QModelIndex &index, PhonicModel *model,
                                         QTableView *tableView) {
        addAction(DeleteWrapCellAction::build(index, model, tableView));
    }

    void WrapCellActions::insertWrapCell(const QModelIndex &index, PhonicModel *model,
                                         QTableView *tableView) {
        addAction(InsertWrapCellAction::build(index, model, tableView));
    }

    void WrapCellActions::warpCellEdit(const QModelIndex &index, PhonicModel *model,
                                       const Phonic &newPhonic) {
        addAction(WrapCellEditAction::build(index, model, newPhonic));
    }

    void WrapCellActions::warpCellChangePhonic(const QModelIndex &index, PhonicModel *model,
                                               const QString &syllableRevised) {
        addAction(WrapCellChangePhonic::build(index, model, syllableRevised));
    }

    void WrapCellActions::warpCellClear(const QModelIndexList &indexes, PhonicModel *model) {
        for (auto &index : indexes) {
            addAction(WrapCellClearAction::build(index, model));
        }
    }
} // FillLyric