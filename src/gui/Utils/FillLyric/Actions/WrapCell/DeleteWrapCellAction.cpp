#include "DeleteWrapCellAction.h"

namespace FillLyric {

    DeleteWrapCellAction *DeleteWrapCellAction::build(const QModelIndex &index, PhonicModel *model,
                                                      QTableView *tableView) {
        auto *action = new DeleteWrapCellAction;
        action->m_model = model;
        action->m_tableView = tableView;

        action->m_cellIndex = index.row() * model->columnCount() + index.column();
        action->m_scrollBarValue = tableView->verticalScrollBar()->value();

        action->m_phonic = model->cellTake(index);
        return action;
    }

    void DeleteWrapCellAction::execute() {
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

        int maxRow = (int) rawPhonics.size() / columnCount;
        if (rawPhonics.size() % columnCount != 0) {
            maxRow++;
        }

        m_model->clear();
        m_model->setRowCount(maxRow);
        m_model->setColumnCount(columnCount);

        for (int i = 0; i < rawPhonics.size(); i++) {
            m_model->putData(i / columnCount, i % columnCount, rawPhonics[i]);
        }

        // 滚动到m_viewPortHeight
        m_tableView->verticalScrollBar()->setValue(m_scrollBarValue);
    }

    void DeleteWrapCellAction::undo() {
        int columnCount = m_model->columnCount();

        QList<Phonic> rawPhonics;
        for (int i = 0; i < m_model->rowCount(); i++) {
            for (int j = 0; j < m_model->columnCount(); j++) {
                if (i == m_model->rowCount() - 1 && m_model->currentLyricLength(i) == j)
                    break;
                rawPhonics.append(m_model->takeData(i, j));
            }
        }

        int rowCount = (int) rawPhonics.size() / columnCount;
        if (rawPhonics.size() % columnCount != 0) {
            rowCount++;
        }

        m_model->clear();
        m_model->setRowCount(rowCount);
        m_model->setColumnCount(columnCount);

        int offset = 0;
        for (int i = 0; i < rawPhonics.size() + 1; i++) {
            if (i == m_cellIndex) {
                m_model->putData(i / columnCount, i % columnCount, m_phonic);
                offset++;
                continue;
            }
            m_model->putData(i / columnCount, i % columnCount, rawPhonics[i - offset]);
        }

        m_tableView->verticalScrollBar()->setValue(m_scrollBarValue);
    }
} // FillLyric