#ifndef PACKAGEINFO_H
#define PACKAGEINFO_H

#include <QVersionNumber>
#include <QString>
#include <QDir>

#include "SingerInfo.h"

namespace srt {
    class PackageRef;
}

class PackageInfo {
public:
    QString id;
    QVersionNumber version;
    QString vendor;
    QString description;
    QString copyright;
    QDir path;

    QList<SingerInfo> singers;
};

Q_DECLARE_METATYPE(PackageInfo)

#endif // PACKAGEINFO_H