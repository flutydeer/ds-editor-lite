#ifndef CANTONESESET_H
#define CANTONESESET_H

#include "../IG2pSetFactory.h"

#include <language-manager/IG2pManager.h>

namespace LangSetting {
    class CantoneseSet final : public IG2pSetFactory {
        Q_OBJECT

    public:
        explicit CantoneseSet(QObject *parent = nullptr) : IG2pSetFactory("yue", parent) {
            d = dynamic_cast<LangMgr::IG2pFactory *>(LangMgr::IG2pManager::instance()->g2p(id()));
        }

        QWidget *configWidget(const QJsonObject &config, bool editable) override;

    private:
        LangMgr::IG2pFactory *d;
    };
}

#endif // CANTONESESET_H