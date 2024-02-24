#include "CellActions.h"

#include "CellClearAction.h"
#include "DeleteCellAction.h"
#include "InsertCellAction.h"
#include "CellMergeLeft.h"
#include "CellEditAction.h"
#include "CellChangePhonic.h"

namespace FillLyric {
    void CellActions::cellClear(const QModelIndexList &indexes, PhonicModel *model) {
        for (const auto &index : indexes) {
            addAction(CellClearAction::build(index, model));
        }
    }

    void CellActions::deleteCell(const QModelIndex &index, PhonicModel *model) {
        addAction(DeleteCellAction::build(index, model));
    }

    void CellActions::insertCell(const QModelIndex &index, PhonicModel *model) {
        addAction(InsertCellAction::build(index, model));
    }

    void CellActions::cellMergeLeft(const QModelIndex &index, PhonicModel *model) {
        addAction(CellMergeLeft::build(index, model));
    }

    void CellActions::cellEdit(const QModelIndex &index, PhonicModel *model,
                               const QList<Phonic> &oldPhonics, const QList<Phonic> &newPhonics) {
        addAction(CellEditAction::build(index, model, oldPhonics, newPhonics));
    }

    void CellActions::cellChangePhonic(const QModelIndex &index, PhonicModel *model,
                                       const QString &syllableRevised) {
        addAction(CellChangePhonic::build(index, model, syllableRevised));
    }

} // FillLyric