#ifndef ADDPREVCELLCMD_H
#define ADDPREVCELLCMD_H

#include "../../View/Controls/CellList.h"

namespace FillLyric {

    class AddPrevCellCmd final : public QUndoCommand {
    public:
        explicit AddPrevCellCmd(CellList *cellList, LyricCell *cell,
                                QUndoCommand *parent = nullptr);
        void undo() override;
        void redo() override;

    private:
        CellList *m_list;
        qlonglong m_index;
        LyricCell *m_newCell;
    };

} // FillLyric

#endif // ADDPREVCELLCMD_H
