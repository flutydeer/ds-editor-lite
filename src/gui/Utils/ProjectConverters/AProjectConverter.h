//
// Created by fluty on 2024/2/7.
//

#ifndef APROJECTCONVERTER_H
#define APROJECTCONVERTER_H

#include "IProjectConverter.h"

class AProjectConverter final : public IProjectConverter {
public:
    bool load(const QString &path, AppModel *model, QString &errMsg) override;
    bool save(const QString &path, AppModel *model, QString &errMsg) override;
};



#endif //APROJECTCONVERTER_H
