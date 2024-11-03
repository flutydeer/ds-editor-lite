#ifndef ENGLISHSET_H
#define ENGLISHSET_H

#include "../IG2pSetFactory.h"

#include <language-manager/ILanguageManager.h>

namespace LangSetting {

    class EnglishSet final : public IG2pSetFactory {
        Q_OBJECT

    public:
        explicit EnglishSet(QObject *parent = nullptr) : IG2pSetFactory("eng", parent) {
            d = LangMgr::ILanguageManager::instance()->g2p(id());
        }

        QWidget *g2pConfigWidget(const QJsonObject &config) override;

    private:
        LangMgr::IG2pFactory *d;
    };

} // LangSetting

#endif // ENGLISHSET_H