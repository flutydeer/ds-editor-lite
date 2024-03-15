#ifndef DS_EDITOR_LITE_WRAPCELLACTIONS_H
#define DS_EDITOR_LITE_WRAPCELLACTIONS_H

#include "../../Model/PhonicModel.h"
#include "../../History/MActionSequence.h"

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
