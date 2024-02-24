#include "WrapLineActions.h"

namespace FillLyric {
    void WrapLineActions::nextWarpLine(const QModelIndex &index, PhonicModel *model) {
        addAction(NextWrapLineAction::build(index, model));
    }

    void WrapLineActions::prevWarpLine(const QModelIndex &index, PhonicModel *model) {
        addAction(PrevWrapLineAction::build(index, model));
    }

    void WrapLineActions::removeWarpLine(const QModelIndex &index, PhonicModel *model) {
        addAction(RemoveWrapLineAction::build(index, model));
    }
} // FillLyric