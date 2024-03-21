//
// Created by fluty on 24-3-21.
//

#ifndef AUDIOOPTION_H
#define AUDIOOPTION_H

#include "Model/AppOptions/IOption.h"

class AudioOption : public IOption {
public:
    explicit AudioOption() : IOption("audio"){}

    void load(const QJsonObject &object) override;

protected:
    void serialize() override;
};



#endif //AUDIOOPTION_H
