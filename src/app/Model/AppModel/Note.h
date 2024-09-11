//
// Created by fluty on 2024/1/27.
//

#ifndef DSNOTE_H
#define DSNOTE_H

#include "Utils/Overlappable.h"
#include "Utils/UniqueObject.h"
// #include "PhonemeInfo.h"
#include "PhonemeInfoSeperated.h"
#include "Pronunciation.h"

#include <QObject>

class SingingClip;

class Note : public QObject, public Overlappable, public UniqueObject, public ISerializable {
    Q_OBJECT

public:
    enum WordPropertyType { Original, Edited };

    explicit Note(SingingClip *context = nullptr, QObject *parent = nullptr);
    ~Note() override;
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
    void setPronunciation(WordPropertyType type, const QString &text);
    [[nodiscard]] QStringList pronCandidates() const;
    void setPronCandidates(const QStringList &pronCandidates);

    // [[nodiscard]] PhonemeInfo phonemeInfo() const;
    // void setPhonemeInfo(WordPropertyType type, const QList<Phoneme> &phonemes);
    // void setPhonemeInfo(const QList<Phoneme> &original, const QList<Phoneme> &edited);
    // void setPhonemeInfo(const PhonemeInfo &info);
    [[nodiscard]] const PhonemeInfoSeperated &phonemes() const;
    void setPhonemes(const PhonemeInfoSeperated &phonemes);

    [[nodiscard]] const PhonemeNameInfo &phonemeNameInfo() const;
    void setPhonemeNameInfo(const PhonemeNameInfo &info);
    void setPhonemeNameInfo(PhonemeInfoSeperated::PhonemeType phType, WordPropertyType wordType,
                            const QList<QString> &nameSeq);

    [[nodiscard]] const PhonemeOffsetInfo &phonemeOffsetInfo() const;
    void setPhonemeOffsetInfo(const PhonemeOffsetInfo &info);
    void setPhonemeOffsetInfo(PhonemeInfoSeperated::PhonemeType phType, WordPropertyType wordType,
                            const QList<int> &offsetSeq);

    [[nodiscard]] QString language() const;
    void setLanguage(const QString &language);
    [[nodiscard]] bool lineFeed() const;
    void setLineFeed(const bool &lineFeed);
    [[nodiscard]] bool isSlur() const;

    [[nodiscard]] QMap<QString, QJsonObject> workspace() const;
    void setWorkspace(const QMap<QString, QJsonObject> &workspace);

    int compareTo(const Note *obj) const;
    [[nodiscard]] std::tuple<qsizetype, qsizetype> interval() const override;

    [[nodiscard]] QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;

    class WordProperties {
    public:
        QString lyric;
        QString language;
        Pronunciation pronunciation;
        QStringList pronCandidates;
        // PhonemeInfo phonemes;
        PhonemeInfoSeperated phonemes;

        static WordProperties fromNote(const Note &note);
    };

private:
    SingingClip *m_clip = nullptr;
    // int m_start = 0;
    int m_rStart = 0;
    int m_length = 480;
    int m_keyIndex = 60;
    QString m_lyric;
    QString m_language = "unknown";
    Pronunciation m_pronunciation;
    QStringList m_pronCandidates;
    // PhonemeInfo m_phonemes;
    PhonemeInfoSeperated m_phonemeInfo;
    bool m_lineFeed = false;
};

#endif // DSNOTE_H