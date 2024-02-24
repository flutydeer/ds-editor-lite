#include "ModelActions.h"

#include "ModelShrinkAction.h"
#include "ToggleFermataAction.h"
#include "WrapToggleFermataAction.h"

namespace FillLyric {

    void ModelActions::shrinkModel(PhonicModel *model) {
        addAction(ModelShrinkAction::build(model));
    }

    void ModelActions::toggleFermata(PhonicModel *model) {
        addAction(ToggleFermataAction::build(model));
    }

    void ModelActions::wrapToggleFermata(PhonicModel *model) {
        addAction(WrapToggleFermataAction::build(model));
    }
} // FillLyric