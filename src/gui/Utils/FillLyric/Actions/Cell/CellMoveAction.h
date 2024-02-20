#ifndef DS_EDITOR_LITE_CELLMOVEACTION_H
#define DS_EDITOR_LITE_CELLMOVEACTION_H

#include "../../Model/PhonicModel.h"
#include "../../History/MAction.h"

#include "../../View/PhonicDelegate.h"

namespace FillLyric {

    class CellMoveAction : public MAction {
    public:
        static CellMoveAction *build(const QModelIndex &source, const QModelIndex &target,
                                     PhonicModel *model);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        QModelIndex m_source;
        QModelIndex m_target;

        Phonic m_targetPhonic;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_CELLMOVEACTION_H
