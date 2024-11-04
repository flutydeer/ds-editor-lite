#ifndef G2PUTIL_H
#define G2PUTIL_H

#include <QMap>

#include "Model/AppOptions/AppOptions.h"

static QMap<QString, QString> languageTog2pId = {
    {"cmn", "cmn-pinyin"},
    {"jpn", "jpn-romaji"},
    {"eng", "eng"       }
};

inline QString g2pIdFromLanguage(const QString &key) {
    return languageTog2pId.contains(key) ? languageTog2pId.value(key) : "unknown";
}

inline QString defaultG2pId() {
    return g2pIdFromLanguage(appOptions->general()->defaultSingingLanguage);
}

static QMap<QString, QString> g2pIdToLanguage = {
    {"cmn-pinyin", "zh"},
    {"jpn-romaji", "ja"},
    {"eng",        "en"}
};

inline QString languageFromG2pId(const QString &key) {
    if (key == "unknown")
        return g2pIdToLanguage.contains(defaultG2pId()) ? g2pIdToLanguage.value(defaultG2pId())
                                                        : "zh";
    return g2pIdToLanguage.contains(key) ? g2pIdToLanguage.value(key) : "zh";
}

#endif // G2PUTIL_H
