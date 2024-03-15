#ifndef UNKNOWN_H
#define UNKNOWN_H

#include "../IG2pFactory.h"

namespace G2pMgr {

    class Unknown final : public IG2pFactory {
        Q_OBJECT
    public:
        explicit Unknown(QObject *parent = nullptr);
        ~Unknown() override;

        QList<Phonic> convert(QStringList &input) const override;
    };

} // G2pMgr

#endif // UNKNOWN_H
