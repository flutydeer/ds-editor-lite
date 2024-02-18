#include "CellChangePhonic.h"

namespace FillLyric {

    CellChangePhonic *CellChangePhonic::build(const QModelIndex &index, PhonicModel *model,
                                              const QString &syllableRevised) {
        auto action = new CellChangePhonic;
        action->m_index = index;
        action->m_model = model;
        action->m_syllableOriginal = model->cellSyllableRevised(index.row(), index.column());
        action->m_syllableRevised = syllableRevised;
        return action;
    }

    void CellChangePhonic::execute() {
        m_model->setSyllableRevised(m_index.row(), m_index.column(), m_syllableRevised);
    }

    void CellChangePhonic::undo() {
        m_model->setSyllableRevised(m_index.row(), m_index.column(), m_syllableOriginal);
    }

} // FillLyric