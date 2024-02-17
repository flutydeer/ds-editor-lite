//
// Created by fluty on 2024/1/27.
//

#include "Note.h"

int Note::start() const {
    return m_start;
}
void Note::setStart(int start) {
    m_start = start;
}
int Note::length() const {
    return m_length;
}
void Note::setLength(int length) {
    m_length = length;
}
int Note::keyIndex() const {
    return m_keyIndex;
}
void Note::setKeyIndex(int keyIndex) {
    m_keyIndex = keyIndex;
}
QString Note::lyric() const {
    return m_lyric;
}
void Note::setLyric(const QString &lyric) {
    m_lyric = lyric;
}
QString Note::pronunciation() const {
    return m_pronunciation;
}
void Note::setPronunciation(const QString &pronunciation) {
    m_pronunciation = pronunciation;
}
Phonemes Note::phonemes() const {
    return m_phonemes;
}
void Note::setPhonemes(Phonemes::PhonemesType type, const QList<Phoneme>& phonemes) {
    if (type == Phonemes::Original)
        m_phonemes.original = phonemes;
    else if (type == Phonemes::Edited)
        m_phonemes.edited = phonemes;

}
bool Note::lineFeed() const {
    return m_lineFeed;
}
void Note::setLineFeed(bool lineFeed) {
    m_lineFeed = lineFeed;
}
bool Note::isSlur() const {
    return m_lyric.contains('-');
}
int Note::compareTo(Note *obj) const {
    auto otherStart = obj->start();
    if (start() < otherStart)
        return -1;
    if (start() > otherStart)
        return 1;
    return 0;
}
bool Note::isOverlappedWith(Note *obj) const {
    auto otherStart = obj->start();
    auto otherEnd = otherStart + obj->length();
    auto curEnd = start() + length();
    if (otherEnd <= start() || curEnd <= otherStart)
        return false;
    return true;
}