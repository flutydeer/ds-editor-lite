#ifndef CANTONESESET_H
#define CANTONESESET_H

#include "../IG2pSetFactory.h"

#include <LangMgr/IG2pManager.h>
#include <G2pMgr/G2p/Cantonese.h>

namespace LangSetting {
    class CantoneseSet final : public IG2pSetFactory {
        Q_OBJECT

    public:
        explicit CantoneseSet(QObject *parent = nullptr) : IG2pSetFactory("yue", parent) {
            d = dynamic_cast<LangMgr::Cantonese *>(LangMgr::IG2pManager::instance()->g2p(id()));
        }

        QWidget *configWidget(QJsonObject *config) override;

    private:
        LangMgr::Cantonese *d;
    };
}

#endif // CANTONESESET_H