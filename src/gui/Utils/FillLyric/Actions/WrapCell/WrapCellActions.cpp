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

    void WrapCellActions::warpCellEdit(const QModelIndex &index, PhonicModel *model,
                                       const QList<Phonic> &oldPhonics,
                                       const QList<Phonic> &newPhonics) {
        addAction(WarpCellEditAction::build(index, model, oldPhonics, newPhonics));
    }

    void WrapCellActions::warpCellChangePhonic(const QModelIndex &index, PhonicModel *model,
                                               const QString &syllableRevised) {
        addAction(WarpCellChangePhonic::build(index, model, syllableRevised));
    }

    void WrapCellActions::warpCellClear(const QModelIndexList &indexes, PhonicModel *model) {
        for (auto &index : indexes) {
            addAction(WarpCellClearAction::build(index, model));
        }
    }
} // FillLyric