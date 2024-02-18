#ifndef DS_EDITOR_LITE_PREVLINEACTION_H
#define DS_EDITOR_LITE_PREVLINEACTION_H

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class PrevLineAction final : public MAction {
    public:
        static PrevLineAction *build(int row, PhonicModel *model);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        int m_newLine = -1;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_PREVLINEACTION_H
