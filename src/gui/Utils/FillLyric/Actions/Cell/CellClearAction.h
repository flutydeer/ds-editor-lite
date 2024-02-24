#ifndef DS_EDITOR_LITE_CELLCLEARACTION_H
#define DS_EDITOR_LITE_CELLCLEARACTION_H

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {
    class CellClearAction final : public MAction {
    public:
        static CellClearAction *build(const QModelIndex &index, PhonicModel *model);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        QModelIndex m_index;
        Phonic m_phonic;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_CELLCLEARACTION_H
