#ifndef ENGLISHSET_H
#define ENGLISHSET_H

#include "../IG2pSetFactory.h"

#include <language-manager/IG2pManager.h>

namespace LangSetting {

    class EnglishSet final : public IG2pSetFactory {
        Q_OBJECT

    public:
        explicit EnglishSet(QObject *parent = nullptr) : IG2pSetFactory("en", parent) {
            d = dynamic_cast<LangMgr::IG2pFactory *>(LangMgr::IG2pManager::instance()->g2p(id()));
        }

        QWidget *configWidget(const QJsonObject &config, bool editable) override;

    private:
        LangMgr::IG2pFactory *d;
    };

} // LangSetting

#endif // ENGLISHSET_H