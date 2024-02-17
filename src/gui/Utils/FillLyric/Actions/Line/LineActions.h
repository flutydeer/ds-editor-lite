#ifndef DS_EDITOR_LITE_LINEACTIONS_H
#define DS_EDITOR_LITE_LINEACTIONS_H

#include <QModelIndex>

#include "LineBreakAction.h"
#include "LineInsertAction.h"

#include "../../Model/PhonicModel.h"
#include "../../History/MActionSequence.h"

namespace FillLyric {

    class LineActions : public MActionSequence {
    public:
        void lineBreak(const QModelIndex &index, PhonicModel *model);
    };

} // FillLyric

#endif // DS_EDITOR_LITE_LINEACTIONS_H
