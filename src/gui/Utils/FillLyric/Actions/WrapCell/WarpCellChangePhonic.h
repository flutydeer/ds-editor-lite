#ifndef DS_EDITOR_LITE_WARPCELLCHANGEPHONIC_H
#define DS_EDITOR_LITE_WARPCELLCHANGEPHONIC_H

#include <QModelIndex>

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class WarpCellChangePhonic final : public MAction {
    public:
        static WarpCellChangePhonic *build(const QModelIndex &index, PhonicModel *model,
                                           const QString &syllableRevised);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        int m_cellIndex;

        QString m_syllableOriginal;
        QString m_syllableRevised;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_WARPCELLCHANGEPHONIC_H
