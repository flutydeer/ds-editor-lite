#ifndef DELETECELLSCMD_H
#define DELETECELLSCMD_H

#include "../../View/Controls/LyricWrapView.h"

namespace FillLyric {

    class DeleteCellsCmd final : public QUndoCommand {
    public:
        explicit DeleteCellsCmd(LyricWrapView *view, QList<LyricCell *> cells,
                                QUndoCommand *parent = nullptr);
        void undo() override;
        void redo() override;

    private:
        LyricWrapView *m_view;
        QMap<CellList *, QMap<int, LyricCell *>> m_cellsMap;
        QList<LyricCell *> m_cells;
    };

} // FillLyric

#endif // DELETECELLSCMD_H
