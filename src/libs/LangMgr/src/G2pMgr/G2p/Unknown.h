#ifndef UNKNOWN_H
#define UNKNOWN_H

#include "../IG2pFactory.h"

namespace G2pMgr {

    class Unknown final : public IG2pFactory {
        Q_OBJECT
    public:
        explicit Unknown(QObject *parent = nullptr);
        ~Unknown() override;

        QList<LangNote> convert(const QStringList &input, const QJsonObject *config) const override;

        QJsonObject config() override;
    };

} // G2pMgr

#endif // UNKNOWN_H
