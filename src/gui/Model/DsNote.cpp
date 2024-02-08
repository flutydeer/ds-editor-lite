//
// Created by fluty on 2024/1/27.
//

#include "DsNote.h"

#include <utility>

int DsNote::start() const {
    return m_start;
}
void DsNote::setStart(int start) {
    m_start = start;
}
int DsNote::length() const {
    return m_length;
}
void DsNote::setLength(int length) {
    m_length = length;
}
int DsNote::keyIndex() const {
    return m_keyIndex;
}
void DsNote::setKeyIndex(int keyIndex) {
    m_keyIndex = keyIndex;
}
QString DsNote::lyric() const {
    return m_lyric;
}
void DsNote::setLyric(const QString &lyric) {
    m_lyric = lyric;
}
QString DsNote::pronunciation() const {
    return m_pronunciation;
}
void DsNote::setPronunciation(const QString &pronunciation) {
    m_pronunciation = pronunciation;
}
DsPhonemes DsNote::phonemes() const {
    return m_phonemes;
}
void DsNote::setPhonemes(DsPhonemes::DsPhonemesType type, QList<DsPhoneme> phonemes) {
    if (type == DsPhonemes::Original)
        m_phonemes.original = phonemes;
    else if (type == DsPhonemes::Edited)
        m_phonemes.edited = phonemes;

}
int DsNote::compareTo(DsNote *obj) const {
    auto otherStart = obj->start();
    if (start() < otherStart)
        return -1;
    if (start() > otherStart)
        return 1;
    return 0;
}
bool DsNote::isOverlappedWith(DsNote *obj) const {
    auto otherStart = obj->start();
    auto otherEnd = otherStart + obj->length();
    auto curEnd = start() + length();
    if (otherEnd <= start() || curEnd <= otherStart)
        return false;
    return true;
}