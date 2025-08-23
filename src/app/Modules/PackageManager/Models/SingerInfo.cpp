#include "SingerInfo.h"

#include <utility>

SingerInfoData::SingerInfoData(SingerIdentifier identifier, QString name,
                               QList<SpeakerInfo> speakers)
    : identifier(std::move(identifier)), name(std::move(name)), speakers(std::move(speakers)) {
}

SingerInfoData::SingerInfoData(const SingerInfoData &other)
    : QSharedData(other), identifier(other.identifier), name(other.name), speakers(other.speakers) {
}

SingerInfoData::~SingerInfoData() = default;

bool SingerInfoData::operator==(const SingerInfoData &other) const {
    return identifier == other.identifier && name == other.name && speakers == other.speakers;
}

bool SingerInfoData::operator!=(const SingerInfoData &other) const {
    return !(*this == other);
}

bool SingerInfoData::isEmpty() const {
    return identifier.isEmpty() && name.isEmpty() && speakers.isEmpty();
}

SingerInfo::SingerInfo() : d(new SingerInfoData()) {
}

SingerInfo::SingerInfo(SingerIdentifier identifier, QString name,
                       QList<SpeakerInfo> speakers)
    : d(new SingerInfoData(std::move(identifier), std::move(name), std::move(speakers))) {
}

SingerInfo::SingerInfo(const SingerInfo &other) = default;

SingerInfo::SingerInfo(SingerInfo &&other) noexcept : d(std::move(other.d)) {
}

SingerInfo &SingerInfo::operator=(const SingerInfo &other) = default;

SingerInfo &SingerInfo::operator=(SingerInfo &&other) noexcept {
    if (this != &other) {
        swap(other);
    }
    return *this;
}

SingerIdentifier SingerInfo::identifier() const {
    return d->identifier;
}

QString SingerInfo::name() const {
    return d->name;
}

QString SingerInfo::singerId() const {
    return d->identifier.singerId;
}

QString SingerInfo::packageId() const {
    return d->identifier.packageId;
}

QVersionNumber SingerInfo::packageVersion() const {
    return d->identifier.packageVersion;
}

QList<SpeakerInfo> SingerInfo::speakers() const {
    return d->speakers;
}

void SingerInfo::setIdentifier(const SingerIdentifier &identifier) {
    d->identifier = identifier;
}

void SingerInfo::setName(const QString &name) {
    d->name = name;
}

void SingerInfo::setSpeakers(const QList<SpeakerInfo> &speakers) {
    d->speakers = speakers;
}

void SingerInfo::addSpeaker(const SpeakerInfo &speaker) {
    d->speakers.append(speaker);
}

bool SingerInfo::isEmpty() const {
    return d->isEmpty();
}

bool SingerInfo::isShared() const {
    return d->ref.loadRelaxed() != 1;
}

void SingerInfo::swap(SingerInfo &other) noexcept {
    qSwap(d, other.d);
}

bool SingerInfo::operator==(const SingerInfo &other) const {
    return d == other.d || *d == *other.d;
}

bool SingerInfo::operator!=(const SingerInfo &other) const {
    return !(*this == other);
}

QString SingerInfo::toString() const {
    QStringList speakers;
    for (const SpeakerInfo &speaker : d->speakers) {
        speakers.append(speaker.toString());
    }

    return QString("SingerInfo(name=%1, identifier=..., speakers=[%2])")
        .arg(d->name)
        .arg(speakers.join(", "));
}

void swap(SingerInfo &first, SingerInfo &second) noexcept {
    first.swap(second);
}