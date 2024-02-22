#ifndef DS_EDITOR_LITE_INSERTWARPCELLACTION_H
#define DS_EDITOR_LITE_INSERTWARPCELLACTION_H

#include <QModelIndex>

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class InsertWarpCellAction final : public MAction {
    public:
        static InsertWarpCellAction *build(const QModelIndex &index, PhonicModel *model);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        bool addRow = false;

        int m_indexRow = 0;
        int m_indexCol = 0;

        int m_modelRowCount = 0;
        int m_modelColumnCount = 0;

        QList<Phonic> m_rawPhonics;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_INSERTWARPCELLACTION_H
