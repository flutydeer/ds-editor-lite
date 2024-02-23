#include "WarpCellChangePhonic.h"

namespace FillLyric {

    WarpCellChangePhonic *WarpCellChangePhonic::build(const QModelIndex &index, PhonicModel *model,
                                                      const QString &syllableRevised) {
        auto action = new WarpCellChangePhonic;
        action->m_cellIndex = index.row() * model->columnCount() + index.column();
        action->m_model = model;

        action->m_syllableOriginal = model->cellSyllableRevised(index.row(), index.column());
        action->m_syllableRevised = syllableRevised;
        return action;
    }

    void WarpCellChangePhonic::execute() {
        int columnCount = m_model->columnCount();
        m_model->setSyllableRevised(m_cellIndex / columnCount, m_cellIndex % columnCount,
                                    m_syllableRevised);
    }

    void WarpCellChangePhonic::undo() {
        int columnCount = m_model->columnCount();
        m_model->setSyllableRevised(m_cellIndex / columnCount, m_cellIndex % columnCount,
                                    m_syllableOriginal);
    }
} // FillLyric