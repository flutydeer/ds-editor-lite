#ifndef MOVEUPLINESCMD_H
#define MOVEUPLINESCMD_H

#include "../../View/Controls/LyricWrapView.h"

namespace FillLyric {

    class MoveUpLinesCmd final : public QUndoCommand {
    public:
        explicit MoveUpLinesCmd(LyricWrapView *view, const QList<CellList *> &lists,
                                QUndoCommand *parent = nullptr);
        void undo() override;
        void redo() override;

    private:
        LyricWrapView *m_view;
        QList<CellList *> m_lists;
    };

} // FillLyric

#endif // MOVEUPLINESCMD_H
