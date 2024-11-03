#ifndef CANTONESESET_H
#define CANTONESESET_H

#include "../IG2pSetFactory.h"

#include <language-manager/ILanguageManager.h>

namespace LangSetting {
    class CantoneseSet final : public IG2pSetFactory {
        Q_OBJECT
    public:
        explicit CantoneseSet(QObject *parent = nullptr) : IG2pSetFactory("yue", parent) {
            d = LangMgr::ILanguageManager::instance()->g2p(id());
        }

        QWidget *configWidget(const QJsonObject &config, bool editable) override;

    private:
        LangMgr::IG2pFactory *d;
    };
}

#endif // CANTONESESET_H