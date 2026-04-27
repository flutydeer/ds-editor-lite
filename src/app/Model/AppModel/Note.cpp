//
// Created by fluty on 2024/1/27.
//

#include "Note.h"

#include "Clip.h"
#include "SingingClip.h"

Note::Note(SingingClip *context, QObject *parent) : QObject(parent), m_clip(context) {
}

Note::~Note() {
    // qDebug() << "~Note()" << id() << m_lyric;
}

SingingClip *Note::clip() const {
    return m_clip;
}

void Note::setClip(SingingClip *clip) {
    m_clip = clip;
}

int Note::globalStart() const {
    if (!m_clip) {
        qFatal() << "SingingClip is null";
        return m_rStart;
    }
    const auto offset = m_clip->start();
    return m_rStart + offset;
}

void Note::setGlobalStart(const int start) {
    if (!m_clip) {
        qFatal() << "SingingClip is null";
        m_rStart = start;
        return;
    }
    const auto offset = m_clip->start();
    const auto rStart = start - offset;
    Q_ASSERT(rStart >= 0);
    m_rStart = rStart;
}

int Note::localStart() const {
    return m_rStart;
}

void Note::setLocalStart(const int rStart) {
    Q_ASSERT(rStart >= 0);
    m_rStart = rStart;
}

int Note::length() const {
    return m_length;
}

void Note::setLength(const int length) {
    Q_ASSERT(length >= 0);
    m_length = length;
}

int Note::keyIndex() const {
    return m_keyIndex;
}

void Note::setKeyIndex(const int keyIndex) {
    Q_ASSERT(keyIndex >= 0 && keyIndex <= 127);
    m_keyIndex = keyIndex;
}

int Note::centShift() const {
    return m_centShift;
}

void Note::setCentShift(const int keyIndex) {
    m_centShift = keyIndex;
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

void Note::setPronunciation(const WordPropertyType type, const QString &text) {
    if (type == Original)
        m_pronunciation.original = text;
    else if (type == Edited)
        m_pronunciation.edited = text;
}

QStringList Note::pronCandidates() const {
    return m_pronCandidates;
}

void Note::setPronCandidates(const QStringList &pronCandidates) {
    m_pronCandidates = pronCandidates;
}

// PhonemeInfo Note::phonemeInfo() const {
//     return m_phonemes;
// }
//
// void Note::setPhonemeInfo(WordPropertyType type, const QList<Phoneme> &phonemes) {
//     if (type == Original)
//         m_phonemes.original = phonemes;
//     else if (type == Edited)
//         m_phonemes.edited = phonemes;
// }
//
// void Note::setPhonemeInfo(const QList<Phoneme> &original, const QList<Phoneme> &edited) {
//     m_phonemes.original = original;
//     m_phonemes.edited = edited;
// }
//
// void Note::setPhonemeInfo(const PhonemeInfo &info) {
//     qCritical() << "Deprecated method setPhonemeInfo() called";
// }

const Phonemes &Note::phonemes() const {
    return m_phonemeInfo;
}

void Note::setPhonemes(const Phonemes &phonemes) {
    m_phonemeInfo = phonemes;
}

const PhonemeNameSeq &Note::phonemeNameSeq() const {
    return m_phonemeInfo.nameSeq;
}

void Note::setPhonemeNameSeq(const PhonemeNameSeq &info) {
    m_phonemeInfo.nameSeq = info;
}

void Note::setPhonemeNameSeq(WordPropertyType wordType, const QList<PhonemeName> &nameSeq) {
    if (wordType == Original)
        m_phonemeInfo.nameSeq.original = nameSeq;
    else if (wordType == Edited)
        m_phonemeInfo.nameSeq.edited = nameSeq;
}

const PhonemeOffsetSeq &Note::phonemeOffsetSeq() const {
    return m_phonemeInfo.offsetSeq;
}

void Note::setPhonemeOffsetSeq(const PhonemeOffsetSeq &info) {
    m_phonemeInfo.offsetSeq = info;
}

void Note::setPhonemeOffsetSeq(WordPropertyType wordType, const QList<int> &offsetSeq) {
    if (wordType == Original)
        m_phonemeInfo.offsetSeq.original = offsetSeq;
    else if (wordType == Edited)
        m_phonemeInfo.offsetSeq.edited = offsetSeq;
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

QMap<QString, QJsonObject> Note::workspace() const {
    QJsonObject objLite{
        {"phoneme", m_phonemeInfo.serialize()}
    };
    return QMap<QString, QJsonObject>{
        {"ds-editor-lite", objLite}
    };
}

void Note::setWorkspace(const QMap<QString, QJsonObject> &workspace) {
    if (workspace.contains("ds-editor-lite")) {
        auto objLite = workspace["ds-editor-lite"];
        if (objLite.contains("phoneme")) {
            const auto objPhoneme = objLite["phoneme"].toObject();
            m_phonemeInfo.deserialize(objPhoneme);
        }
    }
}

int Note::compareTo(const Note *obj) const {
    if (!m_clip) {
        qFatal() << "SingingClip is null";
        return 0;
    }
    if (m_clip != obj->m_clip) {
        qFatal() << "SingingClip is not the same";
        return 0;
    }
    const auto otherStart = obj->localStart();
    if (localStart() < otherStart)
        return -1;
    if (localStart() > otherStart)
        return 1;
    return 0;
}

std::tuple<qsizetype, qsizetype> Note::interval() const {
    return std::make_tuple(localStart(), localStart() + length());
}

QJsonObject Note::serialize() const {
    return {};
}

bool Note::deserialize(const QJsonObject &obj) {
    return false;
}

Note::WordProperties Note::WordProperties::fromNote(const Note &note) {
    WordProperties properties;
    properties.lyric = note.lyric();
    properties.pronunciation = note.pronunciation();
    properties.language = note.language();
    properties.pronCandidates = note.pronCandidates();
    properties.phonemes = note.phonemes();
    return properties;
}