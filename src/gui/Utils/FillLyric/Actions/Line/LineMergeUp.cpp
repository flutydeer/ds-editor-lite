#include "LineMergeUp.h"

namespace FillLyric {
    LineMergeUp *LineMergeUp::build(const QModelIndex &index, PhonicModel *model) {
        auto action = new LineMergeUp;
        action->m_model = model;
        action->m_index = index;
        action->m_deleteLine = index.row();
        action->m_rawColCount = model->columnCount();

        int row = index.row();
        int lastLyricLength = model->currentLyricLength(row - 1);

        int tarColCount = model->currentLyricLength(index.row()) + lastLyricLength - 1;
        tarColCount = qMax(tarColCount, model->columnCount());
        action->m_tarColCount = tarColCount;


        QList<moveInfo> moveList;
        for (int i = 0; i < model->currentLyricLength(row); i++) {
            moveList.append({row, i, row - 1, lastLyricLength + i});
        }

        action->m_moveList = moveList;

        return action;
    }

    void LineMergeUp::execute() {
        if (m_tarColCount > m_rawColCount) {
            m_model->expandModel(m_tarColCount - m_rawColCount);
        }
        for (const auto &info : m_moveList) {
            m_model->moveData(info.srcRow, info.srcCol, info.tarRow, info.tarCol);
        }
        m_model->removeRow(m_deleteLine);
    }

    void LineMergeUp::undo() {
        m_model->insertRow(m_deleteLine);
        for (const auto &info : m_moveList) {
            m_model->moveData(info.tarRow, info.tarCol, info.srcRow, info.srcCol);
        }

        m_tarColCount = m_model->shrinkModel();
    }
} // FillLyric