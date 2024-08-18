#ifndef IG2PFACTORYPRIVATE_H
#define IG2PFACTORYPRIVATE_H

#include <QObject>

#include <LangMgr/IG2pFactory.h>

namespace LangMgr {

    class IG2pFactoryPrivate final : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(IG2pFactory)
    public:
        IG2pFactoryPrivate();
        ~IG2pFactoryPrivate() override;

        void init();

        IG2pFactory *q_ptr;

        QString id;
        QString displayName;
        QString author;
        QString description;
    };

}

#endif // IG2PFACTORYPRIVATE_H