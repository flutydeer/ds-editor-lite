//
// Created by fluty on 2024/1/27.
//

#include "Note.h"

#include "Clip.h"
#include "SingingClip.h"
#include "Utils/JsonUtils.h"

#include <QJsonArray>

Note::Note(SingingClip *context, QObject *parent) : QObject(parent), m_clip(context) {
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
    QJsonObject obj;
    obj["localStart"] = m_rStart;
    obj["length"] = m_length;
    obj["keyIndex"] = m_keyIndex;
    obj["centShift"] = m_centShift;
    obj["lyric"] = m_lyric;
    obj["language"] = m_language;
    obj["pronunciation"] = Pronunciation::serialize(
        Pronunciation({}, m_pronunciation.edited));
    obj["lineFeed"] = m_lineFeed;

    if (!m_pronCandidates.isEmpty()) {
        QJsonArray arr;
        for (const auto &c : m_pronCandidates)
            arr.append(c);
        obj["pronCandidates"] = arr;
    }

    if (m_phonemeInfo.nameSeq.isEdited())
        obj["editedPhonemeNames"] = JsonUtils::serializeList(m_phonemeInfo.nameSeq.edited);

    return obj;
}

bool Note::deserialize(const QJsonObject &obj) {
    m_rStart = obj["localStart"].toInt();
    m_length = obj["length"].toInt();
    m_keyIndex = obj["keyIndex"].toInt();
    m_centShift = obj["centShift"].toInt();
    m_lyric = obj["lyric"].toString();
    m_language = obj["language"].toString("unknown");
    m_lineFeed = obj["lineFeed"].toBool();

    const auto pronObj = obj["pronunciation"].toObject();
    m_pronunciation = Pronunciation({}, pronObj["edited"].toString());

    if (obj.contains("pronCandidates")) {
        m_pronCandidates.clear();
        for (const auto &v : obj["pronCandidates"].toArray())
            m_pronCandidates.append(v.toString());
    }

    if (obj.contains("editedPhonemeNames")) {
        m_phonemeInfo.nameSeq.edited.clear();
        JsonUtils::deserializeList(obj["editedPhonemeNames"].toArray(),
                                   m_phonemeInfo.nameSeq.edited);
    }

    return true;
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