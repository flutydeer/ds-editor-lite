#include "LanguageInfo.h"
#include <QDebug>
#include <utility>

LanguageInfoData::LanguageInfoData() = default;

LanguageInfoData::LanguageInfoData(QString id, QString name, QString g2p, QString dict)
    : id(std::move(id)), name(std::move(name)), g2p(std::move(g2p)), dict(std::move(dict)) {
}

LanguageInfoData::LanguageInfoData(const LanguageInfoData &other)
    : QSharedData(other), id(other.id), name(other.name), g2p(other.g2p), dict(other.dict) {
}

LanguageInfoData::~LanguageInfoData() = default;

bool LanguageInfoData::operator==(const LanguageInfoData &other) const {
    return id == other.id && name == other.name && g2p == other.g2p && dict == other.dict;
}

bool LanguageInfoData::operator!=(const LanguageInfoData &other) const {
    return !(*this == other);
}

bool LanguageInfoData::isEmpty() const {
    return id.isEmpty() && name.isEmpty() && g2p.isEmpty() && dict.isEmpty();
}

LanguageInfo::LanguageInfo() : d(new LanguageInfoData()) {
}

LanguageInfo::LanguageInfo(QString id, QString name, QString g2p, QString dict)
    : d(new LanguageInfoData(std::move(id), std::move(name), std::move(g2p), std::move(dict))) {
}

LanguageInfo::LanguageInfo(const LanguageInfo &other) = default;

LanguageInfo::LanguageInfo(LanguageInfo &&other) noexcept : d(std::move(other.d)) {
}

LanguageInfo &LanguageInfo::operator=(const LanguageInfo &other) = default;

LanguageInfo &LanguageInfo::operator=(LanguageInfo &&other) noexcept {
    if (this != &other) {
        swap(other);
    }
    return *this;
}

bool LanguageInfo::operator==(const LanguageInfo &other) const {
    return d == other.d || *d == *other.d;
}

bool LanguageInfo::operator!=(const LanguageInfo &other) const {
    return !(*this == other);
}

QString LanguageInfo::id() const {
    return d->id;
}

QString LanguageInfo::name() const {
    return d->name;
}

QString LanguageInfo::g2p() const {
    return d->g2p;
}

QString LanguageInfo::dict() const {
    return d->dict;
}

void LanguageInfo::setId(const QString &id) {
    d->id = id;
}

void LanguageInfo::setName(const QString &name) {
    d->name = name;
}

void LanguageInfo::setG2p(const QString &g2p) {
    d->g2p = g2p;
}

void LanguageInfo::setDict(const QString &dict) {
    d->dict = dict;
}

bool LanguageInfo::isEmpty() const {
    return d->isEmpty();
}

bool LanguageInfo::isShared() const {
    return d->ref.loadRelaxed() != 1;
}

void LanguageInfo::swap(LanguageInfo &other) noexcept {
    qSwap(d, other.d);
}

QString LanguageInfo::toString() const {
    return QString("LanguageInfo(id=%1, name=%2, g2p=%3, dict=%4)")
        .arg(d->id, d->name, d->g2p, d->dict);
}

void swap(LanguageInfo &first, LanguageInfo &second) noexcept {
    first.swap(second);
}