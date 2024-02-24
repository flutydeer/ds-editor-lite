#ifndef DS_EDITOR_LITE_LINEMERGEUP_H
#define DS_EDITOR_LITE_LINEMERGEUP_H

#include "../Cell/CellCommon.h"

#include "../../Model/PhonicModel.h"
#include "../../History/MAction.h"

namespace FillLyric {

    class LineMergeUp : public MAction {
    public:
        static LineMergeUp *build(const QModelIndex &index, PhonicModel *model);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        QModelIndex m_index;
        int m_deleteLine = -1;

        int m_rawColCount = -1;
        int m_tarColCount = -1;

        QList<moveInfo> m_moveList;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_LINEMERGEUP_H
