//
// Created by FlutyDeer on 2025/7/27.
//

#ifndef PACKAGE_H
#define PACKAGE_H

#include <QList>
#include <QString>

class InferenceBrief {
public:
    QString id;
    QString className;
    QString configuration;
};

class SingerBrief {
public:
    QString id;
    QString arch;
    QString path;
};

class Dependency {
public:
    QString id;
    QString version;
    bool required;
};

class Package {
public:
    QString id;
    QString version;
    QString vendor;
    QString copyright;
    QString description;
    QString url;
    QList<InferenceBrief> inferences;
    QList<SingerBrief> singers;
    QList<Dependency> dependencies;
};

#endif //PACKAGE_H