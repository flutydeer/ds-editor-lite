#ifndef ENGLISHSET_H
#define ENGLISHSET_H

#include "../IG2pSetFactory.h"

#include <G2pMgr/IG2pManager.h>
#include <G2pMgr/G2p/English.h>

namespace LangSetting {

    class EnglishSet final : public IG2pSetFactory {
        Q_OBJECT

    public:
        explicit EnglishSet(QObject *parent = nullptr) : IG2pSetFactory("en", parent) {
            d = dynamic_cast<G2pMgr::English *>(G2pMgr::IG2pManager::instance()->g2p(id()));
        }

        QWidget *configWidget(QJsonObject *config) override;

    private:
        G2pMgr::English *d;
    };

} // LangSetting

#endif // ENGLISHSET_H