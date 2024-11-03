#ifndef ILANGSETPRIVATE_H
#define ILANGSETPRIVATE_H

#include <QObject>
#include <QMap>

#include "ILangSetManager.h"

namespace LangSetting {

    class ILangSetManagerPrivate final : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(ILangSetManager)
    public:
        ILangSetManagerPrivate();
        ~ILangSetManagerPrivate() override;

        ILangSetManager *q_ptr;

        QMap<QString, IG2pSetFactory *> g2pSets;
    };

}

#endif // ILANGSETPRIVATE_H