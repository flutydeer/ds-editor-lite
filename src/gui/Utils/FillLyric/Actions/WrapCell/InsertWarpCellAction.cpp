#include "InsertWarpCellAction.h"

namespace FillLyric {
    InsertWarpCellAction *InsertWarpCellAction::build(const QModelIndex &index,
                                                      PhonicModel *model) {
        auto action = new InsertWarpCellAction;
        action->m_model = model;
        action->m_indexRow = index.row();
        action->m_indexCol = index.column();

        action->m_modelRowCount = model->rowCount();
        action->m_modelColumnCount = model->columnCount();

        action->addRow =
            !model->cellLyric(model->rowCount() - 1, model->columnCount() - 1).isEmpty();

        QList<Phonic> tempPhonics;
        for (int i = 0; i < model->rowCount(); i++) {
            for (int j = 0; j < model->columnCount(); j++) {
                tempPhonics.append(model->takeData(i, j));
            }
        }
        action->m_rawPhonics = tempPhonics;
        return action;
    }

    void InsertWarpCellAction::execute() {
        int maxRow = addRow ? m_modelRowCount + 1 : m_modelRowCount;

        m_model->clear();
        m_model->setRowCount(maxRow);
        m_model->setColumnCount(m_modelColumnCount);

        int offset = 0;
        for (int i = 0; i < m_rawPhonics.size() + 1; i++) {
            if (i / m_modelColumnCount == m_indexRow && i % m_modelColumnCount == m_indexCol) {
                offset--;
                continue;
            }
            m_model->putData(i / m_modelColumnCount, i % m_modelColumnCount,
                             m_rawPhonics[i + offset]);
        }
    }

    void InsertWarpCellAction::undo() {
        m_model->clear();
        m_model->setRowCount(m_modelRowCount);
        m_model->setColumnCount(m_modelColumnCount);

        for (int i = 0; i < m_rawPhonics.size(); i++) {
            m_model->putData(i / m_modelColumnCount, i % m_modelColumnCount, m_rawPhonics[i]);
        }
    }
} // FillLyric