#ifndef KANA_H
#define KANA_H

#include <QObject>

#include <JapaneseG2p.h>

#include "../IG2pFactory.h"

namespace G2pMgr {

    class Kana final : public IG2pFactory {
        Q_OBJECT
    public:
        explicit Kana(QObject *parent = nullptr);
        ~Kana() override;

        bool initialize(QString &errMsg) override;

        QList<LangNote> convert(const QStringList &input, const QJsonObject *config) const override;

        QJsonObject config() override;

    private:
        IKg2p::JapaneseG2p *m_kana;
    };

} // G2pMgr

#endif // KANA_H
