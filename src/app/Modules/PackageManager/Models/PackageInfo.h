#ifndef PACKAGEINFO_H
#define PACKAGEINFO_H

#include <QVersionNumber>
#include <QString>
#include <QDir>
#include <QList>
#include <QSharedData>
#include <QSharedDataPointer>
#include <QVariant>
#include <utility>

#include "SingerInfo.h"

class PackageInfoData;

class PackageInfo {
public:
    PackageInfo();
    PackageInfo(QString id, QVersionNumber version = {}, QString vendor = {},
                             QString description = {}, QString copyright = {}, QString path = {},
                             QList<SingerInfo> singers = {});
    PackageInfo(const PackageInfo &other);
    PackageInfo(PackageInfo &&other) noexcept;
    PackageInfo &operator=(const PackageInfo &other);
    PackageInfo &operator=(PackageInfo &&other) noexcept;

    QString id() const;
    QVersionNumber version() const;
    QString vendor() const;
    QString description() const;
    QString copyright() const;
    QString path() const;
    QList<SingerInfo> singers() const;

    void setId(const QString &id);
    void setVersion(const QVersionNumber &version);
    void setVendor(const QString &vendor);
    void setDescription(const QString &description);
    void setCopyright(const QString &copyright);
    void setPath(const QString &path);
    void setSingers(const QList<SingerInfo> &singers);

    void addSinger(const SingerInfo &singer);

    bool isEmpty() const;

    bool isShared() const;

    void swap(PackageInfo &other) noexcept;

    QString toString() const;

    bool operator==(const PackageInfo &other) const;
    bool operator!=(const PackageInfo &other) const;

private:
    QSharedDataPointer<PackageInfoData> d;
};

class PackageInfoData : public QSharedData {
public:
    explicit PackageInfoData(QString id = {}, QVersionNumber version = {}, QString vendor = {},
                             QString description = {}, QString copyright = {}, QString path = {},
                             QList<SingerInfo> singers = {});
    PackageInfoData(const PackageInfoData &other);
    ~PackageInfoData();

    QString id;
    QVersionNumber version;
    QString vendor;
    QString description;
    QString copyright;
    QString path;
    QList<SingerInfo> singers;

    bool operator==(const PackageInfoData &other) const;
    bool operator!=(const PackageInfoData &other) const;

    bool isEmpty() const;
};

void swap(PackageInfo &first, PackageInfo &second) noexcept;

Q_DECLARE_METATYPE(PackageInfo)

#endif // PACKAGEINFO_H