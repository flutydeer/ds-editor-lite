#ifndef DS_EDITOR_LITE_WRAPLINEACTIONS_H
#define DS_EDITOR_LITE_WRAPLINEACTIONS_H

#include "NextWrapLineAction.h"

#include "../../Model/PhonicModel.h"
#include "../../History/MActionSequence.h"

namespace FillLyric {

    class WrapLineActions : public MActionSequence {
    public:
        void nextWarpLine(const QModelIndex &index, PhonicModel *model);
    };

} // FillLyric

#endif // DS_EDITOR_LITE_WRAPLINEACTIONS_H
