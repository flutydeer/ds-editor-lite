#include "LineBreakAction.h"

namespace FillLyric {

    LineBreakAction *LineBreakAction::build(const QModelIndex &index, PhonicModel *model) {
        auto action = new LineBreakAction;
        action->m_model = model;
        action->m_index = index;
        action->m_newLine = index.row() + 1;
        action->m_rawColCount = model->columnCount();

        int row = index.row();
        int col = index.column();
        QList<moveInfo> tempMoveList;
        for (int i = col; i < model->currentLyricLength(row); i++) {
            tempMoveList.append({row, i, row + 1, i - col});
        }
        action->m_moveList = tempMoveList;
        return action;
    }

    void LineBreakAction::execute() {
        // 在当前行下方新建一行
        m_model->insertRow(m_newLine);
        // 将当前行col列及之后的内容移动到新行，从新行的第一列开始
        for (const auto &info : m_moveList) {
            m_model->moveData(info.srcRow, info.srcCol, info.tarRow, info.tarCol);
        }
        m_tarColCount = m_model->shrinkModel();
    }

    void LineBreakAction::undo() {
        if (m_rawColCount > m_tarColCount) {
            m_model->expandModel(m_rawColCount - m_tarColCount);
        }
        for (const auto &info : m_moveList) {
            m_model->moveData(info.tarRow, info.tarCol, info.srcRow, info.srcCol);
        }
        m_model->removeRow(m_newLine);
    }
} // FillLyric