#ifndef DS_EDITOR_LITE_S2P_H
#define DS_EDITOR_LITE_S2P_H

#include <Syllable2p.h>
#include "Utils/Singleton.h"

#include <QApplication>

class S2p final : public Singleton<S2p>, public IKg2p::Syllable2p {
public:
    explicit S2p(const QString &dictPath = qApp->applicationDirPath(),
                 const QString &dictName = "Resources/phonemeDict/opencpop-extension.txt",
                 QChar sep1 = '\t', const QString &sep2 = " ")
        : Syllable2p(dictPath.toUtf8().toStdString(), dictName.toUtf8().toStdString(),
                     sep1.unicode(), sep2.toUtf8().toStdString()) {
    }
};

#endif // DS_EDITOR_LITE_S2P_H
