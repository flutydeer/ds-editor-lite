//
// Created by fluty on 2024/1/27.
//

#ifndef DSNOTE_H
#define DSNOTE_H

#include <QList>
#include <QDataStream>
#include <QJsonObject>

#include "Utils/IOverlapable.h"
#include "Utils/ISelectable.h"
#include "Utils/UniqueObject.h"

class Pronunciation {
public:
    QString original;
    QString edited;

    Pronunciation() = default;
    Pronunciation(QString original, QString edited)
        : original(std::move(original)), edited(std::move(edited)) {
    }

    friend QDataStream &operator<<(QDataStream &out, const Pronunciation &pronunciation);
    friend QDataStream &operator>>(QDataStream &in, Pronunciation &pronunciation);

    static QJsonObject serialize(const Pronunciation &pronunciation);
};

class Phoneme {
public:
    enum PhonemeType { Ahead, Normal, Final };

    Phoneme() {
    }
    Phoneme(PhonemeType type, QString name, int start)
        : type(type), name(std::move(name)), start(start) {
    }
    PhonemeType type;
    QString name;
    int start;
};

class Phonemes {
public:
    enum PhonemesType { Original, Edited };
    QList<Phoneme> original;
    QList<Phoneme> edited;

    friend QDataStream &operator<<(QDataStream &out, const Phonemes &phonemes);
    friend QDataStream &operator>>(QDataStream &in, Phonemes &phonemes);

    static QJsonObject serialize(const Phonemes &phonemes);
};

class Note : public IOverlapable, public UniqueObject, public ISelectable {
public:
    explicit Note() = default;
    explicit Note(int start, int length, int keyIndex, QString lyric)
        : m_start(start), m_length(length), m_keyIndex(keyIndex), m_lyric(std::move(lyric)) {
    }

    int start() const;
    void setStart(int start);
    int length() const;
    void setLength(int length);
    int keyIndex() const;
    void setKeyIndex(int keyIndex);
    QString lyric() const;
    void setLyric(const QString &lyric);
    Pronunciation pronunciation() const;
    void setPronunciation(const Pronunciation &pronunciation);
    Phonemes phonemes() const;
    void setPhonemes(Phonemes::PhonemesType type, const QList<Phoneme> &phonemes);
    bool lineFeed() const;
    void setLineFeed(bool lineFeed);
    bool isSlur() const;

    int compareTo(Note *obj) const;
    bool isOverlappedWith(Note *obj) const;

    friend QDataStream &operator<<(QDataStream &out, const Note &note);
    friend QDataStream &operator>>(QDataStream &in, Note &note);

    static QJsonObject serialize(const Note &note);
    static Note deserialize(const QJsonObject &objNote);

    class NoteWordProperties {
    public:
        QString lyric;
        Pronunciation pronunciation;
        Phonemes phonemes;
    };

private:
    int m_start = 0;
    int m_length = 480;
    int m_keyIndex = 60;
    QString m_lyric;
    Pronunciation m_pronunciation;
    Phonemes m_phonemes;
    bool m_lineFeed = false;
};

#endif // DSNOTE_H
