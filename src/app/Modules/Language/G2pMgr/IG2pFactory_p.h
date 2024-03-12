#ifndef IG2PFACTORYPRIVATE_H
#define IG2PFACTORYPRIVATE_H

#include "IG2pFactory.h"

namespace G2pMgr {

    class IG2pFactoryPrivate final {
        Q_DECLARE_PUBLIC(IG2pFactory)
    public:
        IG2pFactoryPrivate();
        virtual ~IG2pFactoryPrivate();

        void init();

        IG2pFactory *q_ptr;

        QString id;

        QString displayName;
        QString description;

        QString iconPath;

        QString category;
        QString displayCategory;
    };

}

#endif // IG2PFACTORYPRIVATE_H