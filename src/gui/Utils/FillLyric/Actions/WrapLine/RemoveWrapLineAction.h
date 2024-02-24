#ifndef DS_EDITOR_LITE_REMOVEWRAPLINEACTION_H
#define DS_EDITOR_LITE_REMOVEWRAPLINEACTION_H

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class RemoveWrapLineAction final : public MAction {
    public:
        static RemoveWrapLineAction *build(const QModelIndex &index, PhonicModel *model);

        void execute() override;

        void undo() override;

    private:
        PhonicModel *m_model;
        QModelIndex m_index;

        int m_startIndex;
        int m_endIndex;

        QList<Phonic> m_phonics;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_REMOVEWRAPLINEACTION_H
