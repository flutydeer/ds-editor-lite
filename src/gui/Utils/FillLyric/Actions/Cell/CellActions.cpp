#include "CellActions.h"

#include "CellClearAction.h"

namespace FillLyric {
    void CellActions::cellClear(const QModelIndexList &indexes, PhonicModel *model) {
        for (const auto &index : indexes) {
            addAction(CellClearAction::build(index, model));
        }
    }
} // FillLyric