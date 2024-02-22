#include "WarpTableAction.h"

namespace FillLyric {

    WarpTableAction *WarpTableAction::build(PhonicModel *model, QTableView *tableView) {
        auto action = new WarpTableAction();
        action->m_model = model;

        action->m_rawRowCount = model->rowCount();
        action->m_rawColCount = model->columnCount();

        QList<Phonic> tempPhonics;
        for (int i = 0; i < model->rowCount(); i++) {
            for (int j = 0; j < model->columnCount(); j++) {
                tempPhonics.append(model->takeData(i, j));
            }
        }

        action->m_rawPhonics = tempPhonics;

        int tableWidth = tableView->width();
        int colWidth = tableView->columnWidth(0);
        auto tarCol = (int) (tableWidth * 0.9 / colWidth);
        auto tarRow = (int) (tempPhonics.size() / tarCol);
        if (tempPhonics.size() % tarCol != 0) {
            tarRow++;
        }

        action->m_tarRow = tarRow;
        action->m_tarCol = tarCol;

        return action;
    }

    void WarpTableAction::execute() {
        m_model->clear();
        m_model->setRowCount(m_tarRow);
        m_model->setColumnCount(m_tarCol);

        for (int i = 0; i < m_tarRow; i++) {
            m_model->insertRow(m_index.row() + 1);
            for (int j = 0; j < m_rawColCount; j++) {
                m_model->putData(m_index.row() + 1, j, m_rawPhonics[i * m_rawColCount + j]);
            }
        }
    }

    void WarpTableAction::undo() {
        m_model->clear();
        m_model->setRowCount(m_rawRowCount);
        m_model->setColumnCount(m_rawColCount);

        for (int i = 0; i < m_rawRowCount; i++) {
            m_model->insertRow(i);
            for (int j = 0; j < m_rawColCount; j++) {
                m_model->putData(i, j, m_rawPhonics[i * m_rawColCount + j]);
            }
        }
    }
} // FillLyric