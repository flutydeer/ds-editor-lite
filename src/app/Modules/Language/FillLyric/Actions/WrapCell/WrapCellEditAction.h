#ifndef DS_EDITOR_LITE_WRAPCELLEDITACTION_H
#define DS_EDITOR_LITE_WRAPCELLEDITACTION_H

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class WrapCellEditAction : public MAction {
    public:
        static WrapCellEditAction *build(const QModelIndex &index, PhonicModel *model,
                                         const Phonic &newPhonic);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        int m_cellIndex;

        Phonic m_oldPhonic;
        Phonic m_newPhonic;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_WRAPCELLEDITACTION_H
