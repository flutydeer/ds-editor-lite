//
// Created by fluty on 2024/1/27.
//

#ifndef DSNOTE_H
#define DSNOTE_H

#include <QList>
#include <utility>

#include "../Utils/IOverlapable.h"
#include "../Utils/UniqueObject.h"

class DsPhoneme {
public:
    enum DsPhonemeType { Ahead, Normal, Final };

    DsPhoneme(DsPhonemeType type, QString name, int start) : type(type), name(std::move(name)), start(start) {
    }
    DsPhonemeType type;
    QString name;
    int start;
};

class DsPhonemes {
public:
    QList<DsPhoneme> original;
    QList<DsPhoneme> edited;
};

class DsNote : public IOverlapable, public UniqueObject {
public:

    explicit DsNote() = default;
    explicit DsNote(int start, int length, int keyIndex, QString lyric)
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
    DsPhonemes phonemes;

    int compareTo(DsNote *obj) const;
    bool isOverlappedWith(DsNote *obj) const;

private:
    int m_start = 0;
    int m_length = 480;
    int m_keyIndex = 60;
    QString m_lyric;
    QString m_pronunciation;
};

#endif // DSNOTE_H
