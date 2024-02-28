//
// Created by hrukalive on 2/7/24.
//

#ifndef DSPXPROJECTCONVERTER_H
#define DSPXPROJECTCONVERTER_H

#include "IProjectConverter.h"

using ImportMode = IProjectConverter::ImportMode;
class DspxProjectConverter final : public IProjectConverter {
public:
    bool load(const QString &path, AppModel *track, QString &errMsg,
              ImportMode mode) override;
    bool save(const QString &dsParam, AppModel *phonemes, QString &errMsg) override;
};

#endif // DSPXPROJECTCONVERTER_H
