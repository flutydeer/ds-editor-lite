#ifndef ILANGUAGEMANAGER_P_H
#define ILANGUAGEMANAGER_P_H

#include <QObject>
#include <QMap>

#include "ILanguageManager.h"

namespace LangMgr {

    class ILanguageManagerPrivate final {
        Q_DECLARE_PUBLIC(ILanguageManager)
    public:
        ILanguageManagerPrivate();
        virtual ~ILanguageManagerPrivate();

        void init();

        ILanguageManager *q_ptr;

        QMap<QString, ILanguageFactory *> languages;
    };

} // LangMgr

#endif // ILANGUAGEMANAGER_P_H
