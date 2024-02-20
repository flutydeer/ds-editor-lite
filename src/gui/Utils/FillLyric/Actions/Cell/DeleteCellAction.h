#ifndef DS_EDITOR_LITE_DELETECELLACTION_H
#define DS_EDITOR_LITE_DELETECELLACTION_H

#include <QObject>
#include <QModelIndex>

#include "CellCommon.h"

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

#include "../../View/PhonicDelegate.h"

namespace FillLyric {
    class DeleteCellAction final : public MAction {
    public:
        static DeleteCellAction *build(const QModelIndex &index, PhonicModel *model);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        QModelIndex m_index;
        Phonic m_phonic;

        QList<moveInfo> m_moveList;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_DELETECELLACTION_H
