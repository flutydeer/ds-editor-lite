#ifndef DS_EDITOR_LITE_LINEBREAKACTION_H
#define DS_EDITOR_LITE_LINEBREAKACTION_H

#include <QObject>
#include <QModelIndex>

#include "../Cell/CellClearAction.h"
#include "../Cell/CellMoveAction.h"

#include "../Line/LineInsertAction.h"

#include "../Model/ModelShrinkAction.h"

#include "../../Model/PhonicModel.h"
#include "../../History/MAction.h"

namespace FillLyric {
    struct moveInfo {
        int srcRow;
        int srcCol;
        int tarRow;
        int tarCol;
    };
    class LineBreakAction : public MAction {
    public:
        static LineBreakAction *build(const QModelIndex &index, PhonicModel *model);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        QModelIndex m_index;
        int newLine = -1;

        int m_rawColCount = -1;
        int m_tarColCount = -1;

        QList<moveInfo> m_moveList;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_LINEBREAKACTION_H
