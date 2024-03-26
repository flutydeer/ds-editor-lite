#ifndef DS_EDITOR_LITE_S2P_H
#define DS_EDITOR_LITE_S2P_H

#include <Syllable2p.h>
#include "Utils/Singleton.h"

#include <QApplication>

class S2p final : public Singleton<S2p>, public IKg2p::Syllable2p {
public:
    explicit S2p(QString phonemeDict = qApp->applicationDirPath() +
                                       "/Resources/phonemeDict/opencpop-extension.txt",
                 QString sep1 = "\t", QString sep2 = " ")
        : Syllable2p(std::move(phonemeDict), std::move(sep1), std::move(sep2)) {
    }
};

#endif // DS_EDITOR_LITE_S2P_H
