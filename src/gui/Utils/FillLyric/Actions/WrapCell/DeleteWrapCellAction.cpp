#include "DeleteWrapCellAction.h"

namespace FillLyric {

    DeleteWrapCellAction *DeleteWrapCellAction::build(const QModelIndex &index,
                                                      PhonicModel *model) {
        auto *action = new DeleteWrapCellAction;
        action->m_model = model;
        action->m_indexRow = index.row();
        action->m_indexCol = index.column();

        action->m_modelRowCount = model->rowCount();
        action->m_modelColumnCount = model->columnCount();

        QList<Phonic> tempPhonics;
        for (int i = 0; i < model->rowCount(); i++) {
            for (int j = 0; j < model->columnCount(); j++) {
                tempPhonics.append(model->takeData(i, j));
            }
        }
        tempPhonics.append(Phonic());
        action->m_rawPhonics = tempPhonics;
        return action;
    }

    void DeleteWrapCellAction::execute() {
        m_model->clear();
        m_model->setRowCount(m_modelRowCount);
        m_model->setColumnCount(m_modelColumnCount);

        int offset = 0;
        for (int i = 0; i < m_rawPhonics.size(); i++) {
            if (i / m_modelColumnCount == m_indexRow && i % m_modelColumnCount == m_indexCol) {
                offset++;
            }
            m_model->putData(i / m_modelColumnCount, i % m_modelColumnCount,
                             m_rawPhonics[i + offset]);
        }
    }

    void DeleteWrapCellAction::undo() {
        m_model->clear();
        m_model->setRowCount(m_modelRowCount);
        m_model->setColumnCount(m_modelColumnCount);

        for (int i = 0; i < m_rawPhonics.size(); i++) {
            m_model->putData(i / m_modelColumnCount, i % m_modelColumnCount, m_rawPhonics[i]);
        }
    }
} // FillLyric