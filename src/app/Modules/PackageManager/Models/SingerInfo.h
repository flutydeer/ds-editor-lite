#ifndef SINGERINFO_H
#define SINGERINFO_H

#include <QString>

#include "Modules/Inference/Models/SingerIdentifier.h"

class PackageInfo;

class SingerInfo {
public:
    SingerIdentifier identifier;
    QString name;
};

Q_DECLARE_METATYPE(SingerInfo)

#endif // SINGERINFO_H