#ifndef DS_EDITOR_LITE_WARPCELLCLEARACTION_H
#define DS_EDITOR_LITE_WARPCELLCLEARACTION_H

#include <QObject>
#include <QModelIndex>

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class WarpCellClearAction final : public MAction {
    public:
        static WarpCellClearAction *build(const QModelIndex &index, PhonicModel *model);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        int m_cellIndex;

        Phonic m_phonic;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_WARPCELLCLEARACTION_H
