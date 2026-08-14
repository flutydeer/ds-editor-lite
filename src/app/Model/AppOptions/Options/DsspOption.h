#ifndef DSSPOPTION_H
#define DSSPOPTION_H

#include "Model/AppOptions/IOption.h"

class DsspOption final : public IOption {
public:
    explicit DsspOption() : IOption("dssp") {};

    void load(const QJsonObject &object) override;
    void save(QJsonObject &object) override;

    LITE_OPTION_ITEM(bool, enabled, true)
    LITE_OPTION_ITEM(QString, host, QStringLiteral("127.0.0.1"))
    LITE_OPTION_ITEM(int, port, 13711)
};


#endif // DSSPOPTION_H
