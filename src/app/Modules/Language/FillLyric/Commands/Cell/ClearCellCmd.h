#ifndef CLEARCELLCMD_H
#define CLEARCELLCMD_H

#include "../../View/Controls/CellList.h"

namespace FillLyric {

    class ClearCellCmd final : public QUndoCommand {
    public:
        explicit ClearCellCmd(CellList *cellList, LyricCell *cell, QUndoCommand *parent = nullptr);
        void undo() override;
        void redo() override;

    private:
        CellList *m_list;
        qlonglong m_index;

        LyricCell *m_cell;
        LyricCell *m_newCell;
    };

} // FillLyric

#endif // CLEARCELLCMD_H
