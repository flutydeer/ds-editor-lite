#ifndef DS_EDITOR_LITE_MACTIONSEQUENCE_H
#define DS_EDITOR_LITE_MACTIONSEQUENCE_H

#include <QList>
#include "MAction.h"

namespace FillLyric {

    class MActionSequence {
    public:
        void execute();
        void undo();
        qsizetype count();

    protected:
        QList<MAction *> m_actionSequence;
        void addAction(MAction *action);
    };

} // FillLyric

#endif // DS_EDITOR_LITE_MACTIONSEQUENCE_H
