#include "InsertWarpCellAction.h"

namespace FillLyric {
    InsertWarpCellAction *InsertWarpCellAction::build(const QModelIndex &index, PhonicModel *model,
                                                      QTableView *tableView) {
        auto action = new InsertWarpCellAction;
        action->m_model = model;
        action->m_tableView = tableView;

        action->m_scrollBarValue = tableView->verticalScrollBar()->value();

        action->m_cellIndex = index.row() * model->columnCount() + index.column();
        return action;
    }

    void InsertWarpCellAction::execute() {
        int columnCount = m_model->columnCount();

        QString lastLyric = m_model->cellLyric(m_model->rowCount() - 1, m_model->columnCount() - 1);
        if (!lastLyric.isEmpty()) {
            m_model->insertRow(m_model->rowCount());
        }

        int rowCount = m_model->rowCount();

        QList<Phonic> rawPhonics;

        for (int i = 0; i < m_model->rowCount(); i++) {
            for (int j = 0; j < m_model->columnCount(); j++) {
                rawPhonics.append(m_model->takeData(i, j));
            }
        }

        m_model->clear();
        m_model->setRowCount(rowCount);
        m_model->setColumnCount(columnCount);

        int offset = 0;
        for (int i = 0; i < rawPhonics.size(); i++) {
            if (i == m_cellIndex) {
                m_model->putData(i / columnCount, i % columnCount, Phonic());
                offset++;
                continue;
            }
            m_model->putData(i / columnCount, i % columnCount, rawPhonics[i - offset]);
        }
        m_tableView->verticalScrollBar()->setValue(m_scrollBarValue);
    }

    void InsertWarpCellAction::undo() {
        int columnCount = m_model->columnCount();

        QList<Phonic> rawPhonics;
        for (int i = 0; i < m_model->rowCount(); i++) {
            for (int j = 0; j < m_model->columnCount(); j++) {
                if (i * columnCount + j == m_cellIndex) {
                    continue;
                }
                if (i == m_model->rowCount() - 1 && m_model->currentLyricLength(i) == j) {
                    break;
                }
                rawPhonics.append(m_model->takeData(i, j));
            }
        }

        int rowCount = m_model->rowCount();
        if ((int) rawPhonics.size() % columnCount == 0) {
            rowCount--;
        }

        m_model->clear();
        m_model->setRowCount(rowCount);
        m_model->setColumnCount(columnCount);

        for (int i = 0; i < rawPhonics.size(); i++) {
            m_model->putData(i / columnCount, i % columnCount, rawPhonics[i]);
        }
        m_tableView->verticalScrollBar()->setValue(m_scrollBarValue);
    }
} // FillLyric