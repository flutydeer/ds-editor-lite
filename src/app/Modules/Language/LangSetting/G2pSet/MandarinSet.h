#ifndef MANDARINSET_H
#define MANDARINSET_H

#include "../IG2pSetFactory.h"

#include <language-manager/ILanguageManager.h>

namespace LangSetting {

    class MandarinSet final : public IG2pSetFactory {
        Q_OBJECT

    public:
        explicit MandarinSet(QObject *parent = nullptr) : IG2pSetFactory("cmn-pinyin", parent) {
            d = LangMgr::ILanguageManager::instance()->g2p(id());
        }

        QWidget *g2pConfigWidget(const QJsonObject &config) override;

    private:
        LangMgr::IG2pFactory *d;
    };

} // LangSetting

#endif // MANDARINSET_H