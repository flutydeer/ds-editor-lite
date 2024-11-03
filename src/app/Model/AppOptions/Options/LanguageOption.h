#ifndef LANGUAGEOPTION_H
#define LANGUAGEOPTION_H

#include "Model/AppOptions/IOption.h"

class LanguageOption final : public IOption {
public:
    explicit LanguageOption() : IOption("language") {
    }

    void load(const QJsonObject &object) override;

    QStringList langOrder;
    QJsonObject g2pConfigs;

protected:
    void save(QJsonObject &object) override;
};



#endif // LANGUAGEOPTION_H
