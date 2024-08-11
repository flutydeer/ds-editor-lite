//
// Created by fluty on 2024/1/27.
//

#ifndef DSNOTE_H
#define DSNOTE_H

#include <QObject>
#include <QList>

#include "Utils/Overlappable.h"
#include "Utils/ISelectable.h"
#include "Utils/UniqueObject.h"


class SingingClip;
class QJsonObject;

class Pronunciation {
public:
    QString original;
    QString edited;

    Pronunciation() = default;

    Pronunciation(QString original, QString edited)
        : original(std::move(original)), edited(std::move(edited)) {
    }

    [[nodiscard]] bool isEdited() const;

    friend QDataStream &operator<<(QDataStream &out, const Pronunciation &pronunciation);
    friend QDataStream &operator>>(QDataStream &in, Pronunciation &pronunciation);

    static QJsonObject serialize(const Pronunciation &pronunciation);
};

class Phoneme {
public:
    enum PhonemeType { Ahead, Normal, Final };

    Phoneme() = default;

    Phoneme(PhonemeType type, QString name, int start)
        : type(type), name(std::move(name)), start(start) {
    }

    static QList<PhonemeType> phonemesTypes();
    PhonemeType type = Normal;
    QString name;
    int start{};
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

class Note : public QObject, public Overlappable, public UniqueObject, public ISelectable {
    Q_OBJECT

public:
    enum NotePropertyType { TimeAndKey, Word, None };

    explicit Note(SingingClip *context = nullptr, QObject *parent = nullptr);
    [[nodiscard]] SingingClip *clip() const;
    void setClip(SingingClip *clip);
    [[nodiscard]] int start() const;
    void setStart(int start);
    [[nodiscard]] int rStart() const;
    void setRStart(int rStart);
    [[nodiscard]] int length() const;
    void setLength(int length);
    [[nodiscard]] int keyIndex() const;
    void setKeyIndex(int keyIndex);
    [[nodiscard]] QString lyric() const;
    void setLyric(const QString &lyric);
    [[nodiscard]] Pronunciation pronunciation() const;
    void setPronunciation(const Pronunciation &pronunciation);
    [[nodiscard]] QStringList pronCandidates() const;
    void setPronCandidates(const QStringList &pronCandidates);
    [[nodiscard]] Phonemes phonemes() const;
    void setPhonemes(Phonemes::PhonemesType type, const QList<Phoneme> &phonemes);
    [[nodiscard]] QString language() const;
    void setLanguage(const QString &language);
    [[nodiscard]] bool lineFeed() const;
    void setLineFeed(const bool &lineFeed);
    [[nodiscard]] bool isSlur() const;

    void notifyPropertyChanged(NotePropertyType type);

    int compareTo(const Note *obj) const;
    bool isOverlappedWith(Note *obj) const;
    [[nodiscard]] std::tuple<qsizetype, qsizetype> interval() const override;

    friend QDataStream &operator<<(QDataStream &out, const Note &note);
    friend QDataStream &operator>>(QDataStream &in, Note &note);

    // static QJsonObject serialize(const Note &note);
    // static Note deserialize(const QJsonObject &objNote);

    class NoteWordProperties {
    public:
        QString lyric;
        QString language;
        Pronunciation pronunciation;
        QStringList pronCandidates;
        Phonemes phonemes;

        static NoteWordProperties fromNote(const Note &note);
    };

signals:
    void propertyChanged(Note::NotePropertyType type);

private:
    SingingClip *m_clip = nullptr;
    //int m_start = 0;
    int m_rStart = 0;
    int m_length = 480;
    int m_keyIndex = 60;
    QString m_lyric;
    QString m_language = "unknown";
    Pronunciation m_pronunciation;
    QStringList m_pronCandidates;
    Phonemes m_phonemes;
    bool m_lineFeed = false;
};

#endif // DSNOTE_H