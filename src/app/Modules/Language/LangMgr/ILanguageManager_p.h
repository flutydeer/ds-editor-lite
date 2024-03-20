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

        bool initialized = false;

        ILanguageManager *q_ptr;

        QStringList category = {"Mandarin", "Cantonese", "Japanese", "English", "Unknown"};

        QStringList order = {"Mandarin", "Pinyin", "Cantonese",   "Kana",   "Romaji",   "English",
                             "Space",    "Slur",   "Punctuation", "Number", "Linebreak"};

        QMap<QString, ILanguageFactory *> languages;
    };

} // LangMgr

#endif // ILANGUAGEMANAGER_P_H
