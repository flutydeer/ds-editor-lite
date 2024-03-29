#include "EditCellCmd.h"

#include <LangMgr/ILanguageManager.h>

namespace FillLyric {
    EditCellCmdfinal::EditCellCmdfinal(CellList *cellList, LyricCell *cell, const QString &lyric,
                                       QUndoCommand *parent)
        : QUndoCommand(parent), m_list(cellList), m_cell(cell) {
        m_oldNote = cell->note();
        m_index = m_list->m_cells.indexOf(cell);
        const auto langMgr = LangMgr::ILanguageManager::instance();

        // TODO: update all notes
        QList<LangNote *> tempNotes;
        for (const auto &lyricCell : m_list->m_cells) {
            tempNotes.append(new LangNote(*lyricCell->note()));
        }
        tempNotes[m_index]->lyric = lyric;
        tempNotes[m_index]->language = langMgr->analysis(lyric);
        tempNotes[m_index]->category = langMgr->language(tempNotes[m_index]->language)->category();

        langMgr->convert(tempNotes);

        m_newNote = new LangNote(*tempNotes.at(m_index));
    }

    void EditCellCmdfinal::undo() {
        m_cell->setNote(m_oldNote);
        m_list->updateRect(m_cell);
        m_cell->update();
    }

    void EditCellCmdfinal::redo() {
        m_cell->setNote(m_newNote);
        m_list->updateRect(m_cell);
        m_cell->update();
    }
} // FillLyric