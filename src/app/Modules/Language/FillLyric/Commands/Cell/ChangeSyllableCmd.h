#ifndef CHANGESYLLABLECMD_H
#define CHANGESYLLABLECMD_H

#include "../../View/Controls/CellList.h"

namespace FillLyric {

    class ChangeSyllableCmd final : public QUndoCommand {
    public:
        explicit ChangeSyllableCmd(CellList *cellList, LyricCell *cell,
                                   const QString &syllableRevised, QUndoCommand *parent = nullptr);
        void undo() override;
        void redo() override;

    private:
        CellList *m_list;
        LyricCell *m_cell;
        qlonglong m_index;

        QString m_syllableRevised;
    };

} // Lyric

#endif // CHANGESYLLABLECMD_H
