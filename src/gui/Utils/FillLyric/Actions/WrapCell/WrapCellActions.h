#ifndef DS_EDITOR_LITE_WRAPCELLACTIONS_H
#define DS_EDITOR_LITE_WRAPCELLACTIONS_H

#include <QModelIndex>
#include "../../History/MActionSequence.h"

#include "DeleteWrapCellAction.h"
#include "InsertWarpCellAction.h"
#include "WarpCellEditAction.h"
#include "WarpCellChangePhonic.h"
#include "WarpCellClearAction.h"

namespace FillLyric {

    class WrapCellActions : public MActionSequence {
    public:
        void deleteWrapCell(const QModelIndex &index, PhonicModel *model, QTableView *tableView);
        void insertWrapCell(const QModelIndex &index, PhonicModel *model, QTableView *tableView);
        void warpCellEdit(const QModelIndex &index, PhonicModel *model,
                          const QList<Phonic> &oldPhonics, const QList<Phonic> &newPhonics);
        void warpCellChangePhonic(const QModelIndex &index, PhonicModel *model,
                                  const QString &syllableRevised);
        void warpCellClear(const QModelIndexList &indexes, PhonicModel *model);
    };

} // FillLyric

#endif // DS_EDITOR_LITE_WRAPCELLACTIONS_H
