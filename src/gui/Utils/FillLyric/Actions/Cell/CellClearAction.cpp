#include "CellClearAction.h"

namespace FillLyric {
    CellClearAction *CellClearAction::build(const QModelIndex &index, PhonicModel *model) {
        auto action = new CellClearAction;
        action->m_model = model;
        action->m_index = index;

        int row = index.row();
        int col = index.column();

        action->m_lyric = model->cellLyric(row, col);
        action->m_syllable = model->cellSyllable(row, col);
        action->m_candidates = model->cellCandidates(row, col);
        action->m_syllableRevised = model->cellSyllableRevised(row, col);
        action->m_type = LyricType(model->cellLyricType(row, col));
        action->m_lineFeed = model->cellLineFeed(row, col);
        return action;
    }

    void CellClearAction::execute() {
        m_model->cellClear(m_index);
    }

    void CellClearAction::undo() {
        int row = m_index.row();
        int col = m_index.column();

        m_model->setLyric(row, col, m_lyric);
        m_model->setSyllable(row, col, m_syllable);
        m_model->setCandidates(row, col, m_candidates);
        m_model->setSyllableRevised(row, col, m_syllableRevised);
        m_model->setLyricType(row, col, m_type);
        m_model->setLineFeed(row, col, m_lineFeed);
    }
} // FillLyric