//
// Created by fluty on 2024/1/27.
//

#include "Note.h"

#include "Clip.h"
#include "SingingClip.h"

#include <QJsonArray>

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

int Note::start() const {
    if (!m_clip) {
        qWarning() << "SingingClip is null";
        return m_rStart;
    }
    auto offset = m_clip->start();
    return m_rStart + offset;
}

void Note::setStart(int start) {
    if (!m_clip) {
        qWarning() << "SingingClip is null";
        m_rStart = start;
        return;
    }
    auto offset = m_clip->start();
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

void Note::setPronunciation(WordPropertyType type, const QString &text) {
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

const PhonemeNameInfo &Note::phonemeNameInfo() const {
    return m_phonemeInfo.nameInfo;
}

void Note::setPhonemeNameInfo(const PhonemeNameInfo &info) {
    m_phonemeInfo.nameInfo = info;
}

void Note::setPhonemeNameInfo(Phonemes::Type phType, WordPropertyType wordType,
                              const QList<QString> &nameSeq) {
    if (phType == Phonemes::Ahead) {
        if (wordType == Original)
            m_phonemeInfo.nameInfo.ahead.original = nameSeq;
        else if (wordType == Edited)
            m_phonemeInfo.nameInfo.ahead.edited = nameSeq;
    } else if (phType == Phonemes::Normal) {
        if (wordType == Original)
            m_phonemeInfo.nameInfo.normal.original = nameSeq;
        else if (wordType == Edited)
            m_phonemeInfo.nameInfo.normal.edited = nameSeq;
    }
}

const PhonemeOffsetInfo &Note::phonemeOffsetInfo() const {
    return m_phonemeInfo.offsetInfo;
}

void Note::setPhonemeOffsetInfo(const PhonemeOffsetInfo &info) {
    m_phonemeInfo.offsetInfo = info;
}

void Note::setPhonemeOffsetInfo(Phonemes::Type phType, WordPropertyType wordType,
                                const QList<int> &offsetSeq) {
    if (phType == Phonemes::Ahead) {
        if (wordType == Original)
            m_phonemeInfo.offsetInfo.ahead.original = offsetSeq;
        else if (wordType == Edited)
            m_phonemeInfo.offsetInfo.ahead.edited = offsetSeq;
    } else if (phType == Phonemes::Normal) {
        if (wordType == Original)
            m_phonemeInfo.offsetInfo.normal.original = offsetSeq;
        else if (wordType == Edited)
            m_phonemeInfo.offsetInfo.normal.edited = offsetSeq;
    }
}

QString Note::language() const {
    return m_language;
}

void Note::setLanguage(const QString &language) {
    m_language = language;
}

QString Note::g2pId() const {
    return m_g2pId;
}

void Note::setG2pId(const QString &g2pId) {
    m_g2pId = g2pId;
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
            auto objPhoneme = objLite["phoneme"].toObject();
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
    const auto otherStart = obj->rStart();
    if (rStart() < otherStart)
        return -1;
    if (rStart() > otherStart)
        return 1;
    return 0;
}

std::tuple<qsizetype, qsizetype> Note::interval() const {
    return std::make_tuple(rStart(), rStart() + length());
}

QJsonObject Note::serialize() const {
    return QJsonObject();
}

bool Note::deserialize(const QJsonObject &obj) {
    return false;
}

Note::WordProperties Note::WordProperties::fromNote(const Note &note) {
    WordProperties properties;
    properties.lyric = note.lyric();
    properties.pronunciation = note.pronunciation();
    properties.language = note.language();
    properties.g2pId = note.g2pId();
    properties.pronCandidates = note.pronCandidates();
    properties.phonemes = note.phonemes();
    return properties;
}