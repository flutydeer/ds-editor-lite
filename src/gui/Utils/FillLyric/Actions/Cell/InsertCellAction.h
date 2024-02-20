#ifndef DS_EDITOR_LITE_INSERTCELLACTION_H
#define DS_EDITOR_LITE_INSERTCELLACTION_H

#include <QObject>
#include <QModelIndex>

#include "CellCommon.h"

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

#include "../../View/PhonicDelegate.h"

namespace FillLyric {

    class InsertCellAction final : public MAction {
    public:
        static InsertCellAction *build(const QModelIndex &index, PhonicModel *model);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        QModelIndex m_index;
        int m_extColumn = 0;

        QList<moveInfo> m_moveList;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_INSERTCELLACTION_H
