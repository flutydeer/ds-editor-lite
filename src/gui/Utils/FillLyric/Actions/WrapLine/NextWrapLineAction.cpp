#include "NextWrapLineAction.h"

namespace FillLyric {
    NextWrapLineAction *NextWrapLineAction::build(const QModelIndex &index, PhonicModel *model) {
        const auto action = new NextWrapLineAction;
        action->m_model = model;
        action->m_index = index;
        action->m_cellIndex = index.row() * model->columnCount() + index.column();

        action->m_startIndex = (index.row() + 1) * model->columnCount();
        action->m_endIndex = action->m_startIndex + model->columnCount();
        return action;
    }

    void NextWrapLineAction::execute() {
        const int col = m_model->columnCount();
        const int row = m_model->rowCount();

        m_model->insertRow(m_index.row() + 1);

        const int currentRow = m_cellIndex / col;
        if (currentRow == row) {
            for (int i = 0; i < col; i++)
                m_model->m_phonics.append(Phonic());
        } else {
            for (int i = m_startIndex; i < m_endIndex; ++i) {
                m_model->insertWarpCell(i, Phonic());
            }
        }
    }

    void NextWrapLineAction::undo() {
        const int col = m_model->columnCount();
        const int row = m_model->rowCount();

        const int currentRow = m_cellIndex / col;
        if (currentRow != row) {
            for (int i = m_startIndex; i < m_endIndex; ++i) {
                m_model->deleteWarpCell(m_startIndex);
            }
        }

        m_model->refreshTable();
    }
} // FillLyric