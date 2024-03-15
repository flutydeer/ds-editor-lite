#ifndef DS_EDITOR_LITE_MODELSHRINKACTION_H
#define DS_EDITOR_LITE_MODELSHRINKACTION_H

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class ModelShrinkAction final : public MAction {
    public:
        static ModelShrinkAction *build(PhonicModel *model);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        int m_rawColCount;
        int m_targetColCount;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_MODELSHRINKACTION_H
