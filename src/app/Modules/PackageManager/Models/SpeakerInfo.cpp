#include "SpeakerInfo.h"
#include <QDebug>
#include <utility>

SpeakerInfoData::SpeakerInfoData() = default;

SpeakerInfoData::SpeakerInfoData(QString id) : id(std::move(id)) {
}

SpeakerInfoData::SpeakerInfoData(QString id, QString name)
    : id(std::move(id)), name(std::move(name)) {
}

SpeakerInfoData::SpeakerInfoData(QString id, QString name, QString toneMin, QString toneMax)
    : id(std::move(id)), name(std::move(name)), toneMin(std::move(toneMin)),
      toneMax(std::move(toneMax)) {
}

SpeakerInfoData::SpeakerInfoData(const SpeakerInfoData &other)
    : QSharedData(other), id(other.id), name(other.name), toneMin(other.toneMin),
      toneMax(other.toneMax) {
}

SpeakerInfoData::~SpeakerInfoData() = default;

bool SpeakerInfoData::operator==(const SpeakerInfoData &other) const {
    return id == other.id && name == other.name && toneMin == other.toneMin &&
           toneMax == other.toneMax;
}

bool SpeakerInfoData::operator!=(const SpeakerInfoData &other) const {
    return !(*this == other);
}

bool SpeakerInfoData::isEmpty() const {
    return id.isEmpty() && name.isEmpty() && toneMin.isEmpty() && toneMax.isEmpty();
}

SpeakerInfo::SpeakerInfo() : d(new SpeakerInfoData()) {
}

SpeakerInfo::SpeakerInfo(const QString &id) : d(new SpeakerInfoData(id)) {
}

SpeakerInfo::SpeakerInfo(const QString &id, const QString &name)
    : d(new SpeakerInfoData(id, name)) {
}

SpeakerInfo::SpeakerInfo(const QString &id, const QString &name, const QString &toneMin,
                         const QString &toneMax)
    : d(new SpeakerInfoData(id, name, toneMin, toneMax)) {
}

SpeakerInfo::SpeakerInfo(const SpeakerInfo &other) = default;

SpeakerInfo::SpeakerInfo(SpeakerInfo &&other) noexcept : d(std::move(other.d)) {
}

SpeakerInfo &SpeakerInfo::operator=(const SpeakerInfo &other) = default;

SpeakerInfo &SpeakerInfo::operator=(SpeakerInfo &&other) noexcept {
    if (this != &other) {
        swap(other);
    }
    return *this;
}

bool SpeakerInfo::operator==(const SpeakerInfo &other) const {
    return d == other.d || *d == *other.d;
}

bool SpeakerInfo::operator!=(const SpeakerInfo &other) const {
    return !(*this == other);
}

QString SpeakerInfo::id() const {
    return d->id;
}

QString SpeakerInfo::name() const {
    return d->name;
}

QString SpeakerInfo::toneMin() const {
    return d->toneMin;
}

QString SpeakerInfo::toneMax() const {
    return d->toneMax;
}

void SpeakerInfo::setId(const QString &id) {
    d->id = id;
}

void SpeakerInfo::setName(const QString &name) {
    d->name = name;
}

void SpeakerInfo::setToneMin(const QString &toneMin) {
    d->toneMin = toneMin;
}

void SpeakerInfo::setToneMax(const QString &toneMax) {
    d->toneMax = toneMax;
}

bool SpeakerInfo::isEmpty() const {
    return d->isEmpty();
}

bool SpeakerInfo::isShared() const {
    return d->ref.loadRelaxed() != 1;
}

void SpeakerInfo::swap(SpeakerInfo &other) noexcept {
    qSwap(d, other.d);
}

QString SpeakerInfo::toString() const {
    return QString("SpeakerInfo(id=%1, name=%2, toneMin=%3, toneMax=%4)")
        .arg(d->id, d->name, d->toneMin, d->toneMax);
}

void swap(SpeakerInfo &first, SpeakerInfo &second) noexcept {
    first.swap(second);
}