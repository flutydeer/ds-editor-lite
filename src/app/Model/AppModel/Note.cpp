//
// Created by fluty on 2024/1/27.
//

#include "Note.h"

#include "Clip.h"

#include <QJsonArray>
#include <QJsonObject>

Note::Note(SingingClip *context, QObject *parent) : QObject(parent), m_clip(context) {
}

SingingClip *Note::clip() const {
    return m_clip;
}
void Note::setClip(SingingClip *clip) {
    m_clip = clip;
}
int Note::start() const {
    auto offset = m_clip ? m_clip->start() : 0;
    return m_rStart + offset;
}
void Note::setStart(int start) {
    auto offset = m_clip ? m_clip->start() : 0;
    auto rStart = start - offset;
    Q_ASSERT(rStart >= 0);
    m_rStart = rStart;
}
int Note::rStart() const {
    return m_rStart;
}
void Note::setRStart(int rStart) {
    Q_ASSERT(rStart >= 0);
    m_rStart = rStart;
}
int Note::length() const {
    return m_length;
}
void Note::setLength(int length) {
    Q_ASSERT(length >= 0);
    m_length = length;
}
int Note::keyIndex() const {
    return m_keyIndex;
}
void Note::setKeyIndex(int keyIndex) {
    Q_ASSERT(keyIndex >= 0 && keyIndex <= 127);
    m_keyIndex = keyIndex;
}
QString Note::lyric() const {
    return m_lyric;
}
void Note::setLyric(const QString &lyric) {
    m_lyric = lyric;
}
Pronunciation Note::pronunciation() const {
    return m_pronunciation;
}
void Note::setPronunciation(const Pronunciation &pronunciation) {
    m_pronunciation = pronunciation;
}
QStringList Note::pronCandidates() const {
    return m_pronCandidates;
}
void Note::setPronCandidates(const QStringList &pronCandidates) {
    m_pronCandidates = pronCandidates;
}
Phonemes Note::phonemes() const {
    return m_phonemes;
}
void Note::setPhonemes(Phonemes::PhonemesType type, const QList<Phoneme> &phonemes) {
    if (type == Phonemes::Original)
        m_phonemes.original = phonemes;
    else if (type == Phonemes::Edited)
        m_phonemes.edited = phonemes;
}
QString Note::language() const {
    return m_language;
}
void Note::setLanguage(const QString &language) {
    m_language = language;
}
bool Note::lineFeed() const {
    return m_lineFeed;
}
void Note::setLineFeed(const bool &lineFeed) {
    m_lineFeed = lineFeed;
}
bool Note::isSlur() const {
    return m_lyric.contains('-');
}
void Note::notifyPropertyChanged(NotePropertyType type) {
    emit propertyChanged(type);
}
int Note::compareTo(const Note *obj) const {
    const auto otherStart = obj->start();
    if (start() < otherStart)
        return -1;
    if (start() > otherStart)
        return 1;
    return 0;
}
bool Note::isOverlappedWith(Note *obj) const {
    const auto otherStart = obj->start();
    const auto otherEnd = otherStart + obj->length();
    const auto curEnd = start() + length();
    if (otherEnd <= start() || curEnd <= otherStart)
        return false;
    return true;
}
std::tuple<qsizetype, qsizetype> Note::interval() const {
    return std::make_tuple(rStart(), rStart() + length());
}
Note::NoteWordProperties Note::NoteWordProperties::fromNote(const Note &note) {
    NoteWordProperties properties;
    properties.lyric = note.lyric();
    properties.pronunciation = note.pronunciation();
    properties.language = note.language();
    properties.pronCandidates = note.pronCandidates();
    properties.phonemes = note.phonemes();
    return properties;
}
// QJsonObject Note::serialize(const Note &note) {
//     QJsonObject objNote;
//     objNote.insert("start", note.start());
//     objNote.insert("length", note.length());
//     objNote.insert("keyIndex", note.keyIndex());
//     objNote.insert("lyric", note.lyric());
//     objNote.insert("language", note.language());
//     objNote.insert("pronunciation", Pronunciation::serialize(note.pronunciation()));
//     objNote.insert("phonemes", Phonemes::serialize(note.phonemes()));
//     return objNote;
// }
// Note Note::deserialize(const QJsonObject &objNote) {
//     Note note;
//     note.setStart(objNote.value("start").toInt());
//     note.setLength(objNote.value("length").toInt());
//     note.setKeyIndex(objNote.value("keyIndex").toInt());
//     note.setLyric(objNote.value("lyric").toString());
//     note.setLanguage(objNote.value("language").toString());
//     // TODO: deserialize pronunciation
//     // TODO: deserialize phonemes
//     return note;
// }
QDataStream &operator<<(QDataStream &out, const Pronunciation &pronunciation) {
    out << pronunciation.original;
    out << pronunciation.edited;
    return out;
}
QDataStream &operator>>(QDataStream &in, Pronunciation &pronunciation) {
    in >> pronunciation.original;
    in >> pronunciation.edited;
    return in;
}
QDataStream &operator<<(QDataStream &out, const Phonemes &phonemes) {
    auto serialize = [](QDataStream &out, const QList<Phoneme> &phonemes) {
        out << phonemes.count();
        for (const auto &phoneme : phonemes) {
            out << phoneme.type;
            out << phoneme.name;
            out << phoneme.start;
        }
    };
    serialize(out, phonemes.original);
    serialize(out, phonemes.edited);
    return out;
}
QDataStream &operator>>(QDataStream &in, Phonemes &phonemes) {
    auto deserialize = [](QDataStream &in, QList<Phoneme> &phonemes) {
        int count;
        in >> count;
        for (int i = 0; i < count; i++) {
            Phoneme phoneme;
            in >> phoneme.type;
            in >> phoneme.name;
            in >> phoneme.start;
            phonemes.append(phoneme);
        }
    };
    deserialize(in, phonemes.original);
    deserialize(in, phonemes.edited);
    return in;
}
bool Pronunciation::isEdited() const {
    return !(edited.isNull() || edited.isEmpty());
}
QJsonObject Pronunciation::serialize(const Pronunciation &pronunciation) {
    QJsonObject objPronunciation;
    objPronunciation.insert("original", pronunciation.original);
    objPronunciation.insert("edited", pronunciation.edited);
    return objPronunciation;
}
QList<Phoneme::PhonemeType> Phoneme::phonemesTypes() {
    return {Ahead, Normal, Final};
}
QJsonObject Phonemes::serialize(const Phonemes &phonemes) {
    QJsonObject objPhonemes;
    auto serializePhoneme = [](QJsonArray &array, const QList<Phoneme> &phonemes) {
        for (const auto &phoneme : phonemes) {
            QJsonObject objPhoneme;
            objPhoneme.insert("type", phoneme.type);
            objPhoneme.insert("name", phoneme.name);
            objPhoneme.insert("start", phoneme.start);
            array.append(objPhoneme);
        }
    };
    QJsonArray arrOriginal;
    QJsonArray arrEdited;
    serializePhoneme(arrOriginal, phonemes.original);
    serializePhoneme(arrEdited, phonemes.edited);
    objPhonemes.insert("original", arrOriginal);
    objPhonemes.insert("edited", arrEdited);
    return objPhonemes;
}

QDataStream &operator<<(QDataStream &out, const Note &note) {
    // out << note.m_id;
    out << note.m_rStart;
    out << note.m_length;
    out << note.m_keyIndex;
    out << note.m_lyric;
    out << note.m_language;
    out << note.m_pronunciation;
    out << note.m_phonemes;
    return out;
}
QDataStream &operator>>(QDataStream &in, Note &note) {
    in >> note.m_rStart;
    in >> note.m_length;
    in >> note.m_keyIndex;
    in >> note.m_lyric;
    in >> note.m_language;
    in >> note.m_pronunciation;
    in >> note.m_phonemes;
    return in;
}