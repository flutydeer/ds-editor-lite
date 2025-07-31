#ifndef G2PUTIL_H
#define G2PUTIL_H

#include <QMap>

#include "Model/AppOptions/AppOptions.h"

static QMap<QString, QString> languageTog2pId = {
    {"cmn", "cmn-pinyin"},
    {"jpn", "jpn-romaji"},
    {"eng", "eng"},
    {"yue", "yue-jyutping"}
};

inline QString g2pIdFromLanguage(const QString &key) {
    return languageTog2pId.contains(key) ? languageTog2pId.value(key) : "unknown";
}

inline QString defaultG2pId() {
    return g2pIdFromLanguage(appOptions->general()->defaultSingingLanguage);
}

static QMap<QString, QString> languageToDict = {
    {"cmn", "zh"},
    {"jpn", "ja"},
    {"eng", "en"},
    {"yue", "yue"}
};

inline QString languageDefaultDictId(const QString &language) {
    return languageToDict.contains(language) ? languageToDict.value(language) : "zh";
}

#endif // G2PUTIL_H