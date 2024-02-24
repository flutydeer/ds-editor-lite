#include "WrapCellChangePhonic.h"

namespace FillLyric {

    WrapCellChangePhonic *WrapCellChangePhonic::build(const QModelIndex &index, PhonicModel *model,
                                                      const QString &syllableRevised) {
        auto action = new WrapCellChangePhonic;
        action->m_cellIndex = index.row() * model->columnCount() + index.column();
        action->m_model = model;

        action->m_oldPhonic = model->m_phonics[action->m_cellIndex];
        Phonic tempPhonic = model->m_phonics[action->m_cellIndex];
        tempPhonic.syllableRevised = syllableRevised;
        action->m_newPhonic = tempPhonic;
        return action;
    }

    void WrapCellChangePhonic::execute() {
        m_model->editWarpCell(m_cellIndex, m_newPhonic);
        m_model->refreshTable();
    }

    void WrapCellChangePhonic::undo() {
        m_model->editWarpCell(m_cellIndex, m_oldPhonic);
        m_model->refreshTable();
    }
} // FillLyric