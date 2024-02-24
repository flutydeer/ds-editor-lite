#include "InsertCellAction.h"

namespace FillLyric {
    InsertCellAction *InsertCellAction::build(const QModelIndex &index, PhonicModel *model) {
        const auto action = new InsertCellAction;
        action->m_model = model;
        action->m_index = index;

        const int row = index.row();
        const int col = index.column();
        // 将对应的单元格的内容移动到右边的单元格，右边单元格的内容依次向右移动，超出范围的部分向右新建单元格
        if (!model->cellLyric(row, -1).isEmpty()) {
            action->m_extColumn = 1;
        }

        QList<moveInfo> moveList;
        for (int i = model->currentLyricLength(row); i > col; i--) {
            moveList.append(moveInfo{row, i - 1, row, i});
        }
        action->m_moveList = moveList;

        return action;
    }

    void InsertCellAction::execute() {
        if (m_extColumn) {
            m_model->setColumnCount(m_model->columnCount() + 1);
        }
        moveExecute(m_moveList, m_model);
    }

    void InsertCellAction::undo() {
        moveUndo(m_moveList, m_model);
        if (m_extColumn) {
            m_model->setColumnCount(m_model->columnCount() - 1);
        }
    }
} // FillLyric