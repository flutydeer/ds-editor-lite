//
// Created by fluty on 2024/1/27.
//

#ifndef DSNOTE_H
#define DSNOTE_H

#include "Utils/Overlappable.h"
#include "Utils/UniqueObject.h"
// #include "PhonemeInfo.h"
#include "Phonemes.h"
#include "Pronunciation.h"

#include <QObject>
#include <QPointer>

class SingingClip;

class Note : public QObject, public Overlappable, public UniqueObject, public ISerializable {
    Q_OBJECT
public:
    enum WordPropertyType { Original, Edited };

    explicit Note(SingingClip *context = nullptr, QObject *parent = nullptr);
    ~Note() override;
    SingingClip *clip() const;
    void setClip(SingingClip *clip);

    int globalStart() const;
    void setGlobalStart(int start);
    int localStart() const;
    void setLocalStart(int rStart);

    int length() const;
    void setLength(int length);
    int keyIndex() const;
    void setKeyIndex(int keyIndex);
    int centShift() const;
    void setCentShift(int keyIndex);

    QString lyric() const;
    void setLyric(const QString &lyric);

    Pronunciation pronunciation() const;
    void setPronunciation(const Pronunciation &pronunciation);
    void setPronunciation(WordPropertyType type, const QString &text);

    QStringList pronCandidates() const;
    void setPronCandidates(const QStringList &pronCandidates);

    //  PhonemeInfo phonemeInfo() const;
    // void setPhonemeInfo(WordPropertyType type, const QList<Phoneme> &phonemes);
    // void setPhonemeInfo(const QList<Phoneme> &original, const QList<Phoneme> &edited);
    // void setPhonemeInfo(const PhonemeInfo &info);

    const Phonemes &phonemes() const;
    void setPhonemes(const Phonemes &phonemes);

    const PhonemeNameInfo &phonemeNameInfo() const;
    void setPhonemeNameInfo(const PhonemeNameInfo &info);
    void setPhonemeNameInfo(Phonemes::Type phType, WordPropertyType wordType,
                            const QList<QString> &nameSeq);

    const PhonemeOffsetInfo &phonemeOffsetInfo() const;
    void setPhonemeOffsetInfo(const PhonemeOffsetInfo &info);
    void setPhonemeOffsetInfo(Phonemes::Type phType, WordPropertyType wordType,
                              const QList<int> &offsetSeq);

    QString language() const;
    void setLanguage(const QString &language);
    bool lineFeed() const;
    void setLineFeed(const bool &lineFeed);
    bool isSlur() const;

    QMap<QString, QJsonObject> workspace() const;
    void setWorkspace(const QMap<QString, QJsonObject> &workspace);

    int compareTo(const Note *obj) const;
    std::tuple<qsizetype, qsizetype> interval() const override;

    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;

    class WordProperties {
    public:
        QString lyric;
        QString language;
        Pronunciation pronunciation;
        QStringList pronCandidates;
        // PhonemeInfo phonemes;
        Phonemes phonemes;

        static WordProperties fromNote(const Note &note);
    };

private:
    QPointer<SingingClip> m_clip;
    // int m_start = 0;
    int m_rStart = 0;
    int m_length = 480;
    int m_keyIndex = 60;
    int m_centShift = 0;

    QString m_lyric;
    QString m_language = "unknown";

    Pronunciation m_pronunciation;
    QStringList m_pronCandidates;
    // PhonemeInfo m_phonemes;
    Phonemes m_phonemeInfo;
    bool m_lineFeed = false;
};

#endif // DSNOTE_H