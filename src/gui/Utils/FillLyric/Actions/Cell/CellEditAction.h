#ifndef DS_EDITOR_LITE_CELLEDITACTION_H
#define DS_EDITOR_LITE_CELLEDITACTION_H

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class CellEditAction : public MAction {
    public:
        static CellEditAction *build(const QModelIndex &index, PhonicModel *model,
                                     const QList<Phonic> &oldPhonics,
                                     const QList<Phonic> &newPhonics);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        QModelIndex m_index;

        QList<Phonic> m_oldPhonics;
        QList<Phonic> m_newPhonics;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_CELLEDITACTION_H
