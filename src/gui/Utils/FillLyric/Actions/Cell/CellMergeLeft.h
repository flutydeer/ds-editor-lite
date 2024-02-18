#ifndef DS_EDITOR_LITE_CELLMERGELEFT_H
#define DS_EDITOR_LITE_CELLMERGELEFT_H

#include <QObject>
#include <QModelIndex>

#include "CellCommon.h"

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

#include "../../Utils/CleanLyric.h"
#include "../../View/PhonicDelegate.h"

namespace FillLyric {

    class CellMergeLeft final : public MAction {
    public:
        static CellMergeLeft *build(const QModelIndex &index, PhonicModel *model);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        QModelIndex m_index;
        QModelIndex m_leftIndex;

        int m_rawColumn = -1;

        Phonic LeftPhonic;
        Phonic currentPhonic;
        QString mergeLyric;

        QList<moveInfo> m_moveList;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_CELLMERGELEFT_H
