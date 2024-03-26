#ifndef ILANGSETFACTORYPRIVATE_H
#define ILANGSETFACTORYPRIVATE_H

#include <QObject>

#include "ILangSetFactory.h"

namespace LangSetting {

    class ILangSetFactoryPrivate final : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(ILangSetFactory)
    public:
        ILangSetFactoryPrivate();
        ~ILangSetFactoryPrivate() override;

        void init();

        ILangSetFactory *q_ptr;

        QString id;
        QString displayName;
        QString author;
        QString description;
    };

}

#endif // ILANGSETFACTORYPRIVATE_H