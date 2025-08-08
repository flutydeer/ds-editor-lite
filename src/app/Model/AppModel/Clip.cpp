//
// Created by fluty on 2024/1/27.
//

#include "Clip.h"

#include <QJsonObject>

QString Clip::name() const {
    return m_name;
}

void Clip::setName(const QString &text) {
    m_name = text;
    // emit propertyChanged();
}

int Clip::start() const {
    return m_start;
}

void Clip::setStart(const int start) {
    m_start = start;
    // emit propertyChanged();
}

int Clip::length() const {
    return m_length;
}

void Clip::setLength(const int length) {
    m_length = length;
    // emit propertyChanged();
}

int Clip::clipStart() const {
    return m_clipStart;
}

void Clip::setClipStart(const int clipStart) {
    m_clipStart = clipStart;
    // emit propertyChanged();
}

int Clip::clipLen() const {
    return m_clipLen;
}

void Clip::setClipLen(const int clipLen) {
    m_clipLen = clipLen;
    // emit propertyChanged();
}

double Clip::gain() const {
    return m_gain;
}

void Clip::setGain(const double gain) {
    m_gain = gain;
    // emit propertyChanged();
}

bool Clip::mute() const {
    return m_mute;
}

void Clip::setMute(const bool mute) {
    m_mute = mute;
    // emit propertyChanged();
}

void Clip::notifyPropertyChanged() {
    emit propertyChanged();
}

QMap<QString, QJsonObject> &Clip::workspace() {
    return m_workspace;
}

QMap<QString, QJsonObject> Clip::workspace() const {
    return m_workspace;
}

int Clip::endTick() const {
    return start() + clipStart() + clipLen();
}

int Clip::compareTo(const Clip *obj) const {
    const auto curVisibleStart = start() + clipStart();
    const auto other = obj;
    const auto otherVisibleStart = other->start() + other->clipStart();
    if (curVisibleStart < otherVisibleStart)
        return -1;
    if (curVisibleStart > otherVisibleStart)
        return 1;
    return 0;
}

bool Clip::isOverlappedWith(const Clip *obj) const {
    const auto curVisibleStart = start() + clipStart();
    const auto curVisibleEnd = curVisibleStart + clipLen();
    const auto other = obj;
    const auto otherVisibleStart = other->start() + other->clipStart();
    const auto otherVisibleEnd = otherVisibleStart + other->clipLen();
    if (otherVisibleEnd <= curVisibleStart || curVisibleEnd <= otherVisibleStart)
        return false;
    return true;
}

std::tuple<qsizetype, qsizetype> Clip::interval() const {
    auto visibleStart = start() + clipStart();
    auto visibleEnd = visibleStart + clipLen();
    return std::make_tuple(visibleStart, visibleEnd);
}

Clip::ClipCommonProperties::ClipCommonProperties(const IClip &clip) {
    applyPropertiesFromClip(*this, clip);
}

void Clip::applyPropertiesFromClip(ClipCommonProperties &args, const IClip &clip) {
    args.name = clip.name();
    args.id = clip.id();
    args.start = clip.start();
    args.clipStart = clip.clipStart();
    args.length = clip.length();
    args.clipLen = clip.clipLen();
    args.gain = clip.gain();
    args.mute = clip.mute();
}