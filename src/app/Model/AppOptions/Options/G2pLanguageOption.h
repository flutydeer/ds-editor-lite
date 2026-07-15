#ifndef G2PLANGUAGEOPTION_H
#define G2PLANGUAGEOPTION_H

#include "Model/AppOptions/IOption.h"

class G2pLanguageOption final : public IOption {
public:
    explicit G2pLanguageOption() : IOption("language") {
    }

    void load(const QJsonObject &object) override;

    QStringList langOrder;

protected:
    void save(QJsonObject &object) override;
};

#endif // G2PLANGUAGEOPTION_H
