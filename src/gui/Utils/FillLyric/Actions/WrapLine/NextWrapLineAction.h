#ifndef DS_EDITOR_LITE_NEXTWRAPLINEACTION_H
#define DS_EDITOR_LITE_NEXTWRAPLINEACTION_H

#include <QModelIndex>

#include "../../Model/PhonicModel.h"
#include "../../History/MAction.h"

namespace FillLyric {

    class NextWrapLineAction final : public MAction {
    public:
        static NextWrapLineAction *build(const QModelIndex &index, PhonicModel *model);

        void execute() override;

        void undo() override;

    private:
        PhonicModel *m_model;
        QModelIndex m_index;

        int m_cellIndex;

        int m_startIndex;
        int m_endIndex;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_NEXTWRAPLINEACTION_H
