#ifndef DS_EDITOR_LITE_DELETEWRAPCELLACTION_H
#define DS_EDITOR_LITE_DELETEWRAPCELLACTION_H

#include <QTableView>
#include <QModelIndex>
#include <QScrollBar>

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class DeleteWrapCellAction final : public MAction {
    public:
        static DeleteWrapCellAction *build(const QModelIndex &index, PhonicModel *model);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;

        QModelIndex m_index;

        int m_cellIndex;

        Phonic m_phonic;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_DELETEWRAPCELLACTION_H
