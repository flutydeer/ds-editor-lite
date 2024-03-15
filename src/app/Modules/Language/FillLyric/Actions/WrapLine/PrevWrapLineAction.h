#ifndef DS_EDITOR_LITE_PREVWRAPLINEACTION_H
#define DS_EDITOR_LITE_PREVWRAPLINEACTION_H

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class PrevWrapLineAction final : public MAction {
    public:
        static PrevWrapLineAction *build(const QModelIndex &index, PhonicModel *model);

        void execute() override;

        void undo() override;

    private:
        PhonicModel *m_model;
        QModelIndex m_index;

        int m_startIndex;
        int m_endIndex;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_PREVWRAPLINEACTION_H
