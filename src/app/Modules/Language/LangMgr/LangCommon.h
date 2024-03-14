#ifndef LANGCOMMON_H
#define LANGCOMMON_H

#include <QString>

namespace LangMgr {
    enum Language { Mandarin, Cantonese, English, Kana, Unknown };
    struct LangNote {
        QString lyric;
        Language language;
    };
}
#endif // LANGCOMMON_H
