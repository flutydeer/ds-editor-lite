//
// Created by fluty on 2024/1/27.
//

#ifndef DSNOTE_H
#define DSNOTE_H

#include <QList>
#include <utility>

#include "Utils/IOverlapable.h"
#include "Utils/UniqueObject.h"

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
};

class Note : public IOverlapable, public UniqueObject {
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
    QString pronunciation() const;
    void setPronunciation(const QString &pronunciation);
    Phonemes phonemes() const;
    void setPhonemes(Phonemes::PhonemesType type, const QList<Phoneme> &phonemes);

    int compareTo(Note *obj) const;
    bool isOverlappedWith(Note *obj) const;

    class NoteWordProperties {
    public:
        QString lyric;
        QString pronunciation;
        Phonemes phonemes;
    };

private:
    int m_start = 0;
    int m_length = 480;
    int m_keyIndex = 60;
    QString m_lyric;
    QString m_pronunciation;
    Phonemes m_phonemes;
};

#endif // DSNOTE_H
