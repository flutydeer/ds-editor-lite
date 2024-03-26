#ifndef MANDARINSET_H
#define MANDARINSET_H

#include "../IG2pSetFactory.h"

#include <G2pMgr/IG2pManager.h>
#include <G2pMgr/G2p/Mandarin.h>

namespace LangSetting {

    class MandarinSet final : public IG2pSetFactory {
        Q_OBJECT
    public:
        explicit MandarinSet(QObject *parent = nullptr) : IG2pSetFactory("Mandarin", parent) {
            d = dynamic_cast<G2pMgr::Mandarin *>(G2pMgr::IG2pManager::instance()->g2p(id()));
        }

        QWidget *configWidget(QJsonObject *config) override;

    private:
        G2pMgr::Mandarin *d;
    };

} // LangSetting

#endif // MANDARINSET_H
