//
// Created by fluty on 2024/2/7.
//

#ifndef APROJECTCONVERTER_H
#define APROJECTCONVERTER_H

#include "IProjectConverter.h"

using ImportMode = IProjectConverter::ImportMode;
class AProjectConverter final : public IProjectConverter {
public:
    bool load(const QString &path, AppModel *model, QString &errMsg,
              ImportMode mode = ImportMode::NewProject) override;
    bool save(const QString &path, AppModel *model, QString &errMsg) override;

private:
    double m_tempo = 120;
};



#endif // APROJECTCONVERTER_H
