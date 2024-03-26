#ifndef IG2pSetFactoryPRIVATE_H
#define IG2pSetFactoryPRIVATE_H

#include <QObject>

#include "IG2pSetFactory.h"

namespace LangSetting {

    class IG2pSetFactoryPrivate final : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(IG2pSetFactory)
    public:
        IG2pSetFactoryPrivate();
        ~IG2pSetFactoryPrivate() override;

        void init();

        IG2pSetFactory *q_ptr;

        QString id;
        QString displayName;
        QString author;
        QString description;
    };

}

#endif // IG2pSetFactoryPRIVATE_H