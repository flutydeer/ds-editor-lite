#ifndef ILANGUAGEMANAGER_P_H
#define ILANGUAGEMANAGER_P_H

#include <QObject>
#include <QMap>

#include "ILanguageManager.h"

namespace LangMgr {

    class ILanguageManagerPrivate final : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(ILanguageManager)

    public:
        ILanguageManagerPrivate();
        ~ILanguageManagerPrivate() override;

        bool initialized = false;

        ILanguageManager *q_ptr;

        QStringList defaultOrder = {"cmn", "cmn-pinyin", "yue", "yue-jyutping", "ja-kana",
                                    "ja-romaji", "en", "space", "slur", "punctuation", "number",
                                    "linebreak", "unknown"};

        QMap<QString, ILanguageFactory *> languages;
    };

} // LangMgr

#endif // ILANGUAGEMANAGER_P_H