#include "SingerInfo.h"

#include <utility>

SingerInfoData::SingerInfoData(SingerIdentifier identifier, QString name,
                               QList<SpeakerInfo> speakers, QList<LanguageInfo> languages,
                               QString defaultLanguage, QString defaultDict)
    : identifier(std::move(identifier)), name(std::move(name)), speakers(std::move(speakers)),
      languages(std::move(languages)), defaultLanguage(std::move(defaultLanguage)),
      defaultDict(std::move(defaultDict)) {
}

SingerInfoData::SingerInfoData(const SingerInfoData &other)
    : QSharedData(other), identifier(other.identifier), name(other.name), speakers(other.speakers),
      languages(other.languages), defaultLanguage(other.defaultLanguage),
      defaultDict(other.defaultDict) {
}

SingerInfoData::~SingerInfoData() = default;

bool SingerInfoData::operator==(const SingerInfoData &other) const {
    return identifier == other.identifier && name == other.name && speakers == other.speakers &&
           languages == other.languages && defaultLanguage == other.defaultLanguage &&
           defaultDict == other.defaultDict;
}

bool SingerInfoData::operator!=(const SingerInfoData &other) const {
    return !(*this == other);
}

bool SingerInfoData::isEmpty() const {
    return identifier.isEmpty() && name.isEmpty() && speakers.isEmpty() && languages.isEmpty() &&
           defaultLanguage.isEmpty() && defaultDict.isEmpty();
}

SingerInfo::SingerInfo() : d(new SingerInfoData()) {
}

SingerInfo::SingerInfo(SingerIdentifier identifier, QString name, QList<SpeakerInfo> speakers,
                       QList<LanguageInfo> languages, QString defaultLanguage, QString defaultDict)
    : d(new SingerInfoData(std::move(identifier), std::move(name), std::move(speakers),
                           std::move(languages), std::move(defaultLanguage),
                           std::move(defaultDict))) {
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

QList<LanguageInfo> SingerInfo::languages() const {
    return d->languages;
}

QString SingerInfo::g2pId(const QString &language) const {
    for (const auto &lang : d->languages) {
        if (language == lang.id())
            return lang.g2p();
    }
    return QStringLiteral("unknown");
}

QString SingerInfo::defaultLanguage() const {
    return d->defaultLanguage;
}

QString SingerInfo::defaultG2pId() const {
    return this->g2pId(this->defaultLanguage());
}

QString SingerInfo::defaultDict() const {
    return d->defaultDict;
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

void SingerInfo::setLanguages(const QList<LanguageInfo> &languages) {
    d->languages = languages;
}

void SingerInfo::setDefaultLanguage(const QString &defaultLanguage) {
    d->defaultLanguage = defaultLanguage;
}

void SingerInfo::setDefaultDict(const QString &defaultDict) {
    d->defaultDict = defaultDict;
}

void SingerInfo::addSpeaker(const SpeakerInfo &speaker) {
    d->speakers.append(speaker);
}

void SingerInfo::addLanguage(const LanguageInfo &language) {
    d->languages.append(language);
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
    speakers.reserve(d->speakers.size());
    for (const SpeakerInfo &speaker : d->speakers) {
        speakers.append(speaker.toString());
    }
    QStringList languages;
    languages.reserve(d->languages.size());
    for (const LanguageInfo &language : d->languages) {
        languages.append(language.toString());
    }

    return QString("SingerInfo(name=%1, identifier=%2, speakers=[%3], "
                   "languages=[%4], defaultLanguage=%5, defaultDict=%6)")
        .arg(d->name)
        .arg(d->identifier.singerId)
        .arg(speakers.join(", "))
        .arg(languages.join(", "))
        .arg(d->defaultLanguage)
        .arg(d->defaultDict);
}

void swap(SingerInfo &first, SingerInfo &second) noexcept {
    first.swap(second);
}