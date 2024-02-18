#ifndef DS_EDITOR_LITE_CELLACTIONS_H
#define DS_EDITOR_LITE_CELLACTIONS_H

#include <QModelIndex>

#include "CellClearAction.h"
#include "DeleteCellAction.h"
#include "InsertCellAction.h"
#include "CellMergeLeft.h"
#include "CellEditAction.h"
#include "CellChangePhonic.h"

#include "../../Model/PhonicModel.h"
#include "../../History/MActionSequence.h"

namespace FillLyric {

    class CellActions : public MActionSequence {
    public:
        void cellClear(const QModelIndexList &indexes, PhonicModel *model);
        void deleteCell(const QModelIndex &index, PhonicModel *model);
        void insertCell(const QModelIndex &index, PhonicModel *model);
        void cellMergeLeft(const QModelIndex &index, PhonicModel *model);
        void cellEdit(const QModelIndex &index, PhonicModel *model, const QList<Phonic> &oldPhonics,
                      const QList<Phonic> &newPhonics);
        void cellChangePhonic(const QModelIndex &index, PhonicModel *model,
                              const QString &syllableRevised);
    };

} // FillLyric

#endif // DS_EDITOR_LITE_CELLACTIONS_H
