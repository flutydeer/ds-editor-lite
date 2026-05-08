//
// Created by fluty on 26-5-8.
//

#ifndef DEVELOPEROPTION_H
#define DEVELOPEROPTION_H

#include "Model/AppOptions/IOption.h"
#include "Utils/Property.h"

class DeveloperOption final : public IOption {
public:
    explicit DeveloperOption() : IOption("developer") {};

    void load(const QJsonObject &object) override;
    void save(QJsonObject &object) override;

    LITE_OPTION_ITEM(bool, enableDiagnostics, false)
};


#endif // DEVELOPEROPTION_H
