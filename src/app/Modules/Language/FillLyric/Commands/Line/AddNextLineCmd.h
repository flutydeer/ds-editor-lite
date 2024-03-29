#ifndef ADDNEXTLINEEDIT_H
#define ADDNEXTLINEEDIT_H

#include "../../View/Controls/LyricWrapView.h"

namespace FillLyric {

    class AddNextLineCmd final : public QUndoCommand {
    public:
        explicit AddNextLineCmd(LyricWrapView *view, CellList *cellList,
                                QUndoCommand *parent = nullptr);
        void undo() override;
        void redo() override;

    private:
        LyricWrapView *m_view;
        qlonglong m_index;
        CellList *m_newList;
    };

} // FillLyric

#endif // ADDNEXTLINEEDIT_H
