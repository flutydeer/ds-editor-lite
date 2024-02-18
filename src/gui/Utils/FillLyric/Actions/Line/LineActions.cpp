#include "LineActions.h"

namespace FillLyric {

    void LineActions::lineBreak(const QModelIndex &index, PhonicModel *model) {
        addAction(LineBreakAction::build(index, model));
    }

    void LineActions::addPrevLine(const QModelIndex &index, PhonicModel *model) {
        addAction(PrevLineAction::build(index.row(), model));
    }

    void LineActions::addNextLine(const QModelIndex &index, PhonicModel *model) {
        addAction(NextLineAction::build(index.row(), model));
    }

    void LineActions::removeLine(const QModelIndex &index, PhonicModel *model) {
        addAction(RemoveLineAction::build(index.row(), model));
    }

    void LineActions::lineMergeUp(const QModelIndex &index, PhonicModel *model) {
        addAction(LineMergeUp::build(index, model));
    }
} // FillLyric