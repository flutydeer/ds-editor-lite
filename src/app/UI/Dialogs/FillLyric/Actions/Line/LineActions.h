#ifndef DS_EDITOR_LITE_LINEACTIONS_H
#define DS_EDITOR_LITE_LINEACTIONS_H

#include "../../Model/PhonicModel.h"
#include "../../History/MActionSequence.h"

namespace FillLyric {

    class LineActions : public MActionSequence {
    public:
        void lineBreak(const QModelIndex &index, PhonicModel *model);
        void addPrevLine(const QModelIndex &index, PhonicModel *model);
        void addNextLine(const QModelIndex &index, PhonicModel *model);
        void removeLine(const QModelIndex &index, PhonicModel *model);
        void lineMergeUp(const QModelIndex &index, PhonicModel *model);
    };

} // FillLyric

#endif // DS_EDITOR_LITE_LINEACTIONS_H
