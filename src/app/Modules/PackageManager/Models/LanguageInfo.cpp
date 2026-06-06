#include "LanguageInfo.h"
#include <QDebug>
#include <utility>

LanguageInfoData::LanguageInfoData() = default;

LanguageInfoData::LanguageInfoData(QString id, QString name, QString g2p, QString dict,
                                   QString s2pMode, QString onsetMode, QString s2pFile,
                                   QString onsetFile)
    : id(std::move(id)), name(std::move(name)), g2p(std::move(g2p)), dict(std::move(dict)),
      s2pMode(std::move(s2pMode)), onsetMode(std::move(onsetMode)),
      s2pFile(std::move(s2pFile)), onsetFile(std::move(onsetFile)) {
}

LanguageInfoData::LanguageInfoData(const LanguageInfoData &other)
    : QSharedData(other), id(other.id), name(other.name), g2p(other.g2p), dict(other.dict),
      s2pMode(other.s2pMode), onsetMode(other.onsetMode), s2pFile(other.s2pFile),
      onsetFile(other.onsetFile) {
}

LanguageInfoData::~LanguageInfoData() = default;

bool LanguageInfoData::operator==(const LanguageInfoData &other) const {
    return id == other.id && name == other.name && g2p == other.g2p && dict == other.dict &&
           s2pMode == other.s2pMode && onsetMode == other.onsetMode &&
           s2pFile == other.s2pFile && onsetFile == other.onsetFile;
}

bool LanguageInfoData::operator!=(const LanguageInfoData &other) const {
    return !(*this == other);
}

bool LanguageInfoData::isEmpty() const {
    return id.isEmpty() && name.isEmpty() && g2p.isEmpty() && dict.isEmpty() &&
           s2pMode.isEmpty() && onsetMode.isEmpty() && s2pFile.isEmpty() &&
           onsetFile.isEmpty();
}

LanguageInfo::LanguageInfo() : d(new LanguageInfoData()) {
}

LanguageInfo::LanguageInfo(QString id, QString name, QString g2p, QString dict,
                           QString s2pMode, QString onsetMode, QString s2pFile,
                           QString onsetFile)
    : d(new LanguageInfoData(std::move(id), std::move(name), std::move(g2p), std::move(dict),
                             std::move(s2pMode), std::move(onsetMode), std::move(s2pFile),
                             std::move(onsetFile))) {
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

QString LanguageInfo::s2pMode() const {
    return d->s2pMode;
}

QString LanguageInfo::onsetMode() const {
    return d->onsetMode;
}

QString LanguageInfo::s2pFile() const {
    return d->s2pFile;
}

QString LanguageInfo::onsetFile() const {
    return d->onsetFile;
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

void LanguageInfo::setS2pMode(const QString &s2pMode) {
    d->s2pMode = s2pMode;
}

void LanguageInfo::setOnsetMode(const QString &onsetMode) {
    d->onsetMode = onsetMode;
}

void LanguageInfo::setS2pFile(const QString &s2pFile) {
    d->s2pFile = s2pFile;
}

void LanguageInfo::setOnsetFile(const QString &onsetFile) {
    d->onsetFile = onsetFile;
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
    return QString("LanguageInfo(id=%1, name=%2, g2p=%3, dict=%4, s2pMode=%5, "
                   "onsetMode=%6, s2pFile=%7, onsetFile=%8)")
        .arg(d->id, d->name, d->g2p, d->dict, d->s2pMode, d->onsetMode, d->s2pFile,
             d->onsetFile);
}

void swap(LanguageInfo &first, LanguageInfo &second) noexcept {
    first.swap(second);
}
