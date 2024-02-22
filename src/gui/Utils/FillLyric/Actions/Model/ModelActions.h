#ifndef DS_EDITOR_LITE_MODELACTIONS_H
#define DS_EDITOR_LITE_MODELACTIONS_H

#include <QModelIndex>

#include "ModelShrinkAction.h"
#include "ToggleFermataAction.h"
#include "WrapToggleFermataAction.h"

#include "../../Model/PhonicModel.h"
#include "../../History/MActionSequence.h"

namespace FillLyric {

    class ModelActions : public MActionSequence {
    public:
        void shrinkModel(PhonicModel *model);
        void toggleFermata(PhonicModel *model);
        void wrapToggleFermata(PhonicModel *model);
    };

} // FillLyric

#endif // DS_EDITOR_LITE_MODELACTIONS_H
