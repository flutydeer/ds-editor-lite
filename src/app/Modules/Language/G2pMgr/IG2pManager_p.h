#ifndef IG2PPRIVATE_H
#define IG2PPRIVATE_H

#include <QObject>
#include <QMap>

#include "IG2pManager.h"

namespace G2pMgr {

    class IG2pManagerPrivate final {
        Q_DECLARE_PUBLIC(IG2pManager)
    public:
        IG2pManagerPrivate();
        virtual ~IG2pManagerPrivate();

        void init();

        IG2pManager *q_ptr;

        QMap<QString, IG2pFactory *> g2ps;

        QStringList baseG2p = {"Slur", "Linebreak", "Number", "Space"};
    };

}

#endif // IG2PPRIVATE_H