#ifndef KANA_H
#define KANA_H

#include <QObject>

#include <jpg2p.h>

#include "../IG2pFactory.h"

namespace G2pMgr {

    class Kana final : public IG2pFactory {
        Q_OBJECT
    public:
        explicit Kana(QObject *parent = nullptr);
        ~Kana() override;

        QList<Phonic> convert(QStringList &input) const override;

    private:
        IKg2p::JpG2p *m_kana;
    };

} // G2pMgr

#endif // KANA_H
