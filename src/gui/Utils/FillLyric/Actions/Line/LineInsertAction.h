#ifndef DS_EDITOR_LITE_LINEINSERTACTION_H
#define DS_EDITOR_LITE_LINEINSERTACTION_H


#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class LineInsertAction final : public MAction {
    public:
        static LineInsertAction *build(int row, PhonicModel *model);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        int m_row = -1;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_LINEINSERTACTION_H
