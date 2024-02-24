#ifndef DS_EDITOR_LITE_WRAPCELLCHANGEPHONIC_H
#define DS_EDITOR_LITE_WRAPCELLCHANGEPHONIC_H

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class WrapCellChangePhonic final : public MAction {
    public:
        static WrapCellChangePhonic *build(const QModelIndex &index, PhonicModel *model,
                                           const QString &syllableRevised);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        int m_cellIndex;

        Phonic m_oldPhonic;
        Phonic m_newPhonic;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_WRAPCELLCHANGEPHONIC_H
