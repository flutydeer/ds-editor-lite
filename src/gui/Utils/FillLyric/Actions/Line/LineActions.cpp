#include "LineActions.h"

namespace FillLyric {

    void LineActions::lineBreak(const QModelIndex &index, PhonicModel *model) {
        addAction(LineBreakAction::build(index, model));
    }
} // FillLyric