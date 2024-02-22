#ifndef DS_EDITOR_LITE_WRAPTOGGLEFERMATAACTION_H
#define DS_EDITOR_LITE_WRAPTOGGLEFERMATAACTION_H

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class WrapToggleFermataAction final : public MAction {
    public:
        static WrapToggleFermataAction *build(PhonicModel *model);
        void execute();
        void undo();

    private:
        PhonicModel *m_model = nullptr;

        bool m_fermataState;
        int m_modelColumnCount;

        QList<Phonic> m_rawPhonics;
        QList<Phonic> m_targetPhonics;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_WRAPTOGGLEFERMATAACTION_H
