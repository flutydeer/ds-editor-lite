#ifndef DS_EDITOR_LITE_INSERTWRAPCELLACTION_H
#define DS_EDITOR_LITE_INSERTWRAPCELLACTION_H

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class InsertWrapCellAction final : public MAction {
    public:
        static InsertWrapCellAction *build(const QModelIndex &index, PhonicModel *model);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;

        int m_cellIndex;
        Phonic m_phonic;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_INSERTWRAPCELLACTION_H
