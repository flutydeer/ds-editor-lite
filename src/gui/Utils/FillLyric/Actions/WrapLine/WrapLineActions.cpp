#include "WrapLineActions.h"

namespace FillLyric {
    void WrapLineActions::nextWarpLine(const QModelIndex &index, PhonicModel *model) {
        addAction(NextWrapLineAction::build(index, model));
    }
} // FillLyric