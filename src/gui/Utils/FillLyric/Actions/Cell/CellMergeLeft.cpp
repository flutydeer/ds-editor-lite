#include "CellMergeLeft.h"

namespace FillLyric {

    CellMergeLeft *CellMergeLeft::build(const QModelIndex &index, PhonicModel *model) {
        const auto action = new CellMergeLeft;
        action->m_index = index;
        action->m_model = model;
        action->m_rawColumn = model->columnCount();

        const int row = index.row();
        const int col = index.column();
        action->m_leftIndex = model->index(row, col - 1);
        action->LeftPhonic = model->takeData(row, col - 1);
        action->currentPhonic = model->takeData(row, col);
        action->mergeLyric = action->LeftPhonic.lyric + action->currentPhonic.lyric;

        QList<moveInfo> moveList;
        for (int i = col; i < model->currentLyricLength(row); i++) {
            moveList.append(moveInfo{row, i + 1, row, i});
        }
        action->m_moveList = moveList;

        return action;
    }

    void CellMergeLeft::execute() {
        m_model->setLyric(m_index.row(), m_index.column() - 1, mergeLyric);
        moveExecute(m_moveList, m_model);
        m_model->repaintItem(m_leftIndex, mergeLyric);
        m_model->shrinkModel();
    }

    void CellMergeLeft::undo() {
        m_model->setColumnCount(m_rawColumn);
        moveUndo(m_moveList, m_model);
        m_model->putData(m_index.row(), m_index.column() - 1, LeftPhonic);
        m_model->putData(m_index.row(), m_index.column(), currentPhonic);
    }
} // FillLyric