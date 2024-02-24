#ifndef DS_EDITOR_LITE_WRAPCELLACTIONS_H
#define DS_EDITOR_LITE_WRAPCELLACTIONS_H

#include <QModelIndex>
#include "../../History/MActionSequence.h"

#include "DeleteWrapCellAction.h"
#include "InsertWrapCellAction.h"
#include "WrapCellEditAction.h"
#include "WrapCellChangePhonic.h"
#include "WrapCellClearAction.h"

namespace FillLyric {

    class WrapCellActions : public MActionSequence {
    public:
        void deleteWrapCell(const QModelIndex &index, PhonicModel *model);
        void insertWrapCell(const QModelIndex &index, PhonicModel *model);
        void warpCellEdit(const QModelIndex &index, PhonicModel *model, const Phonic &newPhonic);
        void warpCellChangePhonic(const QModelIndex &index, PhonicModel *model,
                                  const QString &syllableRevised);
        void warpCellClear(const QModelIndexList &indexes, PhonicModel *model);
    };

} // FillLyric

#endif // DS_EDITOR_LITE_WRAPCELLACTIONS_H
