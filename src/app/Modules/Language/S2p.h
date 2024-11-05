#ifndef DS_EDITOR_LITE_S2P_H
#define DS_EDITOR_LITE_S2P_H

#include <QApplication>

#include "S2p/Syllable2p.h"
#include "Utils/Singleton.h"

#include "Model/AppOptions/AppOptions.h"

class S2p final : public Singleton<S2p>, public FillLyric::Syllable2p {
public:
    explicit S2p(const QString &dictPath = QApplication::applicationDirPath(),
                 const QString &dictName = getDefaultDictName())
        : Syllable2p(dictPath, dictName) {
    }

private:
    // TODO: support multi-dict
    static QString getDefaultDictName() {
        if (appOptions->general()->defaultSingingLanguage == "cmn") {
#ifdef Q_OS_MAC
            return QStringLiteral("../Resources/phonemeDict/opencpop-extension.txt");
#else
            return QStringLiteral("Resources/phonemeDict/opencpop-extension.txt");
#endif
        } else {
#ifdef Q_OS_MAC
            return QStringLiteral("../Resources/phonemeDict/japanese_dict_full.txt");
#else
            return QStringLiteral("Resources/phonemeDict/japanese_dict_full.txt");
#endif
        }
    }
};

#endif // DS_EDITOR_LITE_S2P_H