#ifndef CANTONESESET_H
#define CANTONESESET_H

#include "../IG2pSetFactory.h"

#include <G2pMgr/IG2pManager.h>
#include <G2pMgr/G2p/Cantonese.h>

namespace LangSetting {
    class CantoneseSet final : public IG2pSetFactory {
        Q_OBJECT
    public:
        explicit CantoneseSet(QObject *parent = nullptr) : IG2pSetFactory("Cantonese", parent) {
            d = dynamic_cast<G2pMgr::Cantonese *>(G2pMgr::IG2pManager::instance()->g2p(id()));
        }

        QWidget *configWidget(QJsonObject *config) override;

    private:
        G2pMgr::Cantonese *d;
    };
}

#endif // CANTONESESET_H
