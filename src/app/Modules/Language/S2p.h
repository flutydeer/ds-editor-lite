#ifndef DS_EDITOR_LITE_S2P_H
#define DS_EDITOR_LITE_S2P_H

#include <QApplication>

#include "S2p/Syllable2p.h"
#include "Utils/Singleton.h"

class S2p final : public Singleton<S2p>, public FillLyric::Syllable2p {
public:
    explicit S2p(const QString &dictPath = qApp->applicationDirPath(),
                 const QString &dictName = "Resources/phonemeDict/opencpop-extension.txt",
                 QChar sep1 = '\t', const QString &sep2 = " ")
        : Syllable2p(dictPath, dictName, sep1, sep2) {
    }
};

#endif // DS_EDITOR_LITE_S2P_H