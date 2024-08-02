//
// Created by fluty on 24-8-2.
//

#ifndef GENERALOPTION_H
#define GENERALOPTION_H

#include "Global/AppGlobal.h"
#include "Model/AppOptions/IOption.h"

class GeneralOption final : public IOption {
public:
    explicit GeneralOption() : IOption("general"){};

    void load(const QJsonObject &object) override;
    void save(QJsonObject &object) override;

    AppGlobal::languageType defaultSingingLanguage = AppGlobal::Mandarin;
    QString defaultLyric = "啦";

private:
    const QString defaultSingingLanguageKey = "defaultSingLanguage";
    const QString defaultLyricKey = "defaultLyric";
};



#endif // GENERALOPTION_H
