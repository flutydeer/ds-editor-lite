#ifndef G2PCOMMON_H
#define G2PCOMMON_H

#include "Model/Note.h"

namespace G2pMgr {

    struct Phonic {
        QString text;
        Pronunciation pronunciation;
        QList<QString> candidates;
        bool isSlur = false;
        bool error = false;
    };

};

#endif // G2PCOMMON_H
