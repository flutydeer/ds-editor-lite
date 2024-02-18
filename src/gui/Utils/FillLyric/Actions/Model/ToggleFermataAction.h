#ifndef DS_EDITOR_LITE_TOGGLEFERMATAACTION_H
#define DS_EDITOR_LITE_TOGGLEFERMATAACTION_H

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class ToggleFermataAction final : public MAction {
    public:
        static ToggleFermataAction *build(PhonicModel *model);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_TOGGLEFERMATAACTION_H
