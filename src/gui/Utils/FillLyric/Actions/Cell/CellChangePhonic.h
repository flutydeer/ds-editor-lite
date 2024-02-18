#ifndef DS_EDITOR_LITE_CELLCHANGEPHONIC_H
#define DS_EDITOR_LITE_CELLCHANGEPHONIC_H

#include <QObject>
#include <QModelIndex>

#include "../../History/MAction.h"
#include "../../Model/PhonicModel.h"

namespace FillLyric {

    class CellChangePhonic : public MAction {
    public:
        static CellChangePhonic *build(const QModelIndex &index, PhonicModel *model,
                                       const QString &syllableRevised);
        void execute() override;
        void undo() override;

    private:
        PhonicModel *m_model = nullptr;
        QModelIndex m_index;

        QString m_syllableOriginal;
        QString m_syllableRevised;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_CELLCHANGEPHONIC_H
