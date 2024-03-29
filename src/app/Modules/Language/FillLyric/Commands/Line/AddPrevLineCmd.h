#ifndef ADDPREVLINECMD_H
#define ADDPREVLINECMD_H

#include "../../View/Controls/LyricWrapView.h"

namespace FillLyric {

    class AddPrevLineCmd final : public QUndoCommand {
    public:
        explicit AddPrevLineCmd(LyricWrapView *view, CellList *cellList,
                                QUndoCommand *parent = nullptr);
        void undo() override;
        void redo() override;

    private:
        LyricWrapView *m_view;
        qlonglong m_index;
        CellList *m_newList;
    };

} // FillLyric

#endif // ADDPREVLINECMD_H
