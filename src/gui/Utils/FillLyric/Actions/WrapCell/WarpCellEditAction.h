#ifndef DS_EDITOR_LITE_WARPCELLEDITACTION_H
#define DS_EDITOR_LITE_WARPCELLEDITACTION_H

#include <QObject>
#include <QModelIndex>

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class WarpCellEditAction : public MAction {
    public:
        static WarpCellEditAction *build(const QModelIndex &index, PhonicModel *model,
                                         const QList<Phonic> &oldPhonics,
                                         const QList<Phonic> &newPhonics);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        int m_startIndex;
        int m_endIndex;

        QList<Phonic> m_oldPhonics;
        QList<Phonic> m_newPhonics;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_WARPCELLEDITACTION_H
