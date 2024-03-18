#ifndef IG2PFACTORYPRIVATE_H
#define IG2PFACTORYPRIVATE_H

#include <QObject>

#include "IG2pFactory.h"

namespace G2pMgr {

    class IG2pFactoryPrivate final : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(IG2pFactory)
    public:
        IG2pFactoryPrivate();
        virtual ~IG2pFactoryPrivate();

        void init();

        IG2pFactory *q_ptr;

        QString id;

        QString author;
        QString description;
    };

}

#endif // IG2PFACTORYPRIVATE_H