#ifndef DS_EDITOR_LITE_WARPTABLEACTION_H
#define DS_EDITOR_LITE_WARPTABLEACTION_H

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class WarpTableAction final : public MAction {
    public:
        static WarpTableAction *build(PhonicModel *model, QTableView *tableView);
        void execute();
        void undo();

    private:
        WarpTableAction();

        PhonicModel *m_model;
        QTableView *m_tableView;

        QModelIndex m_index;

        int m_rawRowCount;
        int m_rawColCount;

        int m_tarRow;
        int m_tarCol;

        QList<Phonic> m_rawPhonics;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_WARPTABLEACTION_H
