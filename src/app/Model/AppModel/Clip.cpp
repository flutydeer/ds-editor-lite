//
// Created by fluty on 2024/1/27.
//

#include "Clip.h"

#include <QDebug>

#include "Note.h"
#include "Curve.h"
#include "DrawCurve.h"
#include "Model/Inference/InferPiece.h"
#include "Utils/AppModelUtils.h"
#include "Utils/Linq.h"
#include "Utils/MathUtils.h"

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

void Clip::setStart(int start) {
    m_start = start;
    // emit propertyChanged();
}

int Clip::length() const {
    return m_length;
}

void Clip::setLength(int length) {
    m_length = length;
    // emit propertyChanged();
}

int Clip::clipStart() const {
    return m_clipStart;
}

void Clip::setClipStart(int clipStart) {
    m_clipStart = clipStart;
    // emit propertyChanged();
}

int Clip::clipLen() const {
    return m_clipLen;
}

void Clip::setClipLen(int clipLen) {
    m_clipLen = clipLen;
    // emit propertyChanged();
}

double Clip::gain() const {
    return m_gain;
}

void Clip::setGain(double gain) {
    m_gain = gain;
    // emit propertyChanged();
}

bool Clip::mute() const {
    return m_mute;
}

void Clip::setMute(bool mute) {
    m_mute = mute;
    // emit propertyChanged();
}

void Clip::notifyPropertyChanged() {
    emit propertyChanged();
}

int Clip::endTick() const {
    return start() + clipStart() + clipLen();
}

int Clip::compareTo(const Clip *obj) const {
    auto curVisibleStart = start() + clipStart();
    auto other = obj;
    auto otherVisibleStart = other->start() + other->clipStart();
    if (curVisibleStart < otherVisibleStart)
        return -1;
    if (curVisibleStart > otherVisibleStart)
        return 1;
    return 0;
}

bool Clip::isOverlappedWith(Clip *obj) const {
    auto curVisibleStart = start() + clipStart();
    auto curVisibleEnd = curVisibleStart + clipLen();
    auto other = obj;
    auto otherVisibleStart = other->start() + other->clipStart();
    auto otherVisibleEnd = otherVisibleStart + other->clipLen();
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

AudioClip::AudioClipProperties::AudioClipProperties(const AudioClip &clip) {
    applyPropertiesFromClip(*this, clip);
    path = clip.path();
}

AudioClip::AudioClipProperties::AudioClipProperties(const IClip &clip) {
    applyPropertiesFromClip(*this, clip);
}

AudioClip::~AudioClip() {
    qDebug() << "~AudioClip()" << id() << m_name;
}