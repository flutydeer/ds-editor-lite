#ifndef ENGLISH_H
#define ENGLISH_H

#include "../IG2pFactory.h"

namespace G2pMgr {

    class English final : public IG2pFactory {
        Q_OBJECT
    public:
        explicit English(QObject *parent = nullptr);
        ~English() override;

        QList<Phonic> convert(QStringList &input) const override;

        QJsonObject config() override;
        QWidget *configWidget(QJsonObject *config) override;

    private:
        bool toLower = true;
    };

} // G2pMgr

#endif // ENGLISH_H
