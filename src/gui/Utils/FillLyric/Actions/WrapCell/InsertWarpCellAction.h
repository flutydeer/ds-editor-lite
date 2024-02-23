#ifndef DS_EDITOR_LITE_INSERTWARPCELLACTION_H
#define DS_EDITOR_LITE_INSERTWARPCELLACTION_H

#include <QTableView>
#include <QModelIndex>
#include <QScrollBar>

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class InsertWarpCellAction final : public MAction {
    public:
        static InsertWarpCellAction *build(const QModelIndex &index, PhonicModel *model,
                                           QTableView *tableView);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        QTableView *m_tableView = nullptr;

        int m_scrollBarValue;
        int m_cellIndex;

        Phonic m_phonic;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_INSERTWARPCELLACTION_H
