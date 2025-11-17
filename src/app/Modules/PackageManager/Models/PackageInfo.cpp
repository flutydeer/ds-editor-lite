#include "PackageInfo.h"

#include <utility>

PackageInfoData::PackageInfoData(QString id, QVersionNumber version, QString vendor,
                                 QString description, QString copyright, QString readme,
                                 QString url, QString path, QList<SingerInfo> singers)
    : id(std::move(id)), version(std::move(version)), vendor(std::move(vendor)),
      description(std::move(description)), copyright(std::move(copyright)),
      readme(std::move(readme)), url(std::move(url)), path(std::move(path)),
      singers(std::move(singers)) {
}

PackageInfoData::PackageInfoData(const PackageInfoData &other)
    : QSharedData(other), id(other.id), version(other.version), vendor(other.vendor),
      description(other.description), copyright(other.copyright),
      readme(other.readme), url(other.url), path(other.path),
      singers(other.singers) {
}

PackageInfoData::~PackageInfoData() = default;

bool PackageInfoData::operator==(const PackageInfoData &other) const {
    return id == other.id && version == other.version && vendor == other.vendor &&
           description == other.description && copyright == other.copyright &&
           readme == other.readme && url == other.url && path == other.path &&
           singers == other.singers;
}

bool PackageInfoData::operator!=(const PackageInfoData &other) const {
    return !(*this == other);
}

bool PackageInfoData::isEmpty() const {
    return id.isEmpty() && version.isNull() && vendor.isEmpty() && description.isEmpty() &&
           copyright.isEmpty() && readme.isEmpty() && url.isEmpty() && path.isEmpty() &&
           singers.isEmpty();
}

PackageInfo::PackageInfo() : d(new PackageInfoData()) {
}

PackageInfo::PackageInfo(QString id, QVersionNumber version, QString vendor, QString description,
                         QString copyright, QString readme, QString url, QString path,
                         QList<SingerInfo> singers)
    : d(new PackageInfoData(std::move(id), std::move(version), std::move(vendor),
                            std::move(description), std::move(copyright), std::move(readme),
                            std::move(url), std::move(path), std::move(singers))) {
}

PackageInfo::PackageInfo(const PackageInfo &other) = default;

PackageInfo::PackageInfo(PackageInfo &&other) noexcept : d(std::move(other.d)) {
}

PackageInfo &PackageInfo::operator=(const PackageInfo &other) = default;

PackageInfo &PackageInfo::operator=(PackageInfo &&other) noexcept {
    if (this != &other) {
        swap(other);
    }
    return *this;
}

QString PackageInfo::id() const {
    return d->id;
}

QVersionNumber PackageInfo::version() const {
    return d->version;
}

QString PackageInfo::vendor() const {
    return d->vendor;
}

QString PackageInfo::description() const {
    return d->description;
}

QString PackageInfo::copyright() const {
    return d->copyright;
}

QString PackageInfo::readme() const {
    return d->readme;
}

QString PackageInfo::url() const {
    return d->url;
}

QString PackageInfo::path() const {
    return d->path;
}

QList<SingerInfo> PackageInfo::singers() const {
    return d->singers;
}

void PackageInfo::setId(const QString &id) {
    d->id = id;
}

void PackageInfo::setVersion(const QVersionNumber &version) {
    d->version = version;
}

void PackageInfo::setVendor(const QString &vendor) {
    d->vendor = vendor;
}

void PackageInfo::setDescription(const QString &description) {
    d->description = description;
}

void PackageInfo::setCopyright(const QString &copyright) {
    d->copyright = copyright;
}

void PackageInfo::setPath(const QString &path) {
    d->path = path;
}

void PackageInfo::setSingers(const QList<SingerInfo> &singers) {
    d->singers = singers;
}

void PackageInfo::addSinger(const SingerInfo &singer) {
    d->singers.append(singer);
}

bool PackageInfo::isEmpty() const {
    return d->isEmpty();
}

bool PackageInfo::isShared() const {
    return d->ref.loadRelaxed() != 1;
}

void PackageInfo::swap(PackageInfo &other) noexcept {
    qSwap(d, other.d);
}

bool PackageInfo::operator==(const PackageInfo &other) const {
    return d == other.d || *d == *other.d;
}

bool PackageInfo::operator!=(const PackageInfo &other) const {
    return !(*this == other);
}

QString PackageInfo::toString() const {
    QStringList singerNames;
    for (const SingerInfo &singer : d->singers) {
        singerNames.append(singer.name());
    }

    return QString("PackageInfo(id=%1, version=%2, vendor=%3, description=%4, singers=[%5])")
        .arg(d->id)
        .arg(d->version.toString())
        .arg(d->vendor)
        .arg(d->description)
        .arg(singerNames.join(", "));
}

void swap(PackageInfo &first, PackageInfo &second) noexcept {
    first.swap(second);
}