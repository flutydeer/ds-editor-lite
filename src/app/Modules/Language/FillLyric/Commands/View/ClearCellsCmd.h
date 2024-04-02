#ifndef CLEARCELLSCMD_H
#define CLEARCELLSCMD_H

#include "../../View/Controls/LyricWrapView.h"

namespace FillLyric {

    class ClearCellsCmd final : public QUndoCommand {
    public:
        explicit ClearCellsCmd(LyricWrapView *view, QList<LyricCell *> cells,
                               QUndoCommand *parent = nullptr);
        void undo() override;
        void redo() override;

    private:
        struct cell {
            LyricCell *m_new, *m_old;
        };

        LyricWrapView *m_view;
        QMap<CellList *, QMap<int, cell>> m_cellsMap;
        QList<LyricCell *> m_cells;
    };

} // FillLyric

#endif // CLEARCELLSCMD_H
