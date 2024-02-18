#ifndef DS_EDITOR_LITE_REMOVELINEACTION_H
#define DS_EDITOR_LITE_REMOVELINEACTION_H

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class RemoveLineAction final : public MAction {
    public:
        static RemoveLineAction *build(int row, PhonicModel *model);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        int m_rmvLine = -1;
        QList<Phonic> m_phonics;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_REMOVELINEACTION_H
