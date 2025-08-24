#ifndef SINGERINFO_H
#define SINGERINFO_H

#include <QString>
#include <QList>
#include <QSharedData>
#include <QSharedDataPointer>
#include <QVariant>

#include "SpeakerInfo.h"
#include "LanguageInfo.h"
#include "Modules/Inference/Models/SingerIdentifier.h"

class SingerInfoData;

class SingerInfo {
public:
    SingerInfo();
    explicit SingerInfo(SingerIdentifier identifier, QString name = {},
                        QList<SpeakerInfo> speakers = {}, QList<LanguageInfo> languages = {},
                        QString defaultLanguage = {}, QString defaultDict = {});
    SingerInfo(const SingerInfo &other);
    SingerInfo(SingerInfo &&other) noexcept;
    SingerInfo &operator=(const SingerInfo &other);
    SingerInfo &operator=(SingerInfo &&other) noexcept;

    SingerIdentifier identifier() const;
    QString name() const;
    QString singerId() const;
    QString packageId() const;
    QVersionNumber packageVersion() const;
    QList<SpeakerInfo> speakers() const;
    QList<LanguageInfo> languages() const;
    QString defaultLanguage() const;
    QString defaultDict() const;

    void setIdentifier(const SingerIdentifier &identifier);
    void setName(const QString &name);
    void setSpeakers(const QList<SpeakerInfo> &speakers);
    void setLanguages(const QList<LanguageInfo> &languages);
    void setDefaultLanguage(const QString &defaultLanguage);
    void setDefaultDict(const QString &defaultDict);

    void addSpeaker(const SpeakerInfo &speaker);
    void addLanguage(const LanguageInfo &language);

    bool isEmpty() const;

    bool isShared() const;

    void swap(SingerInfo &other) noexcept;

    QString toString() const;

    bool operator==(const SingerInfo &other) const;
    bool operator!=(const SingerInfo &other) const;

private:
    QSharedDataPointer<SingerInfoData> d;
};

class SingerInfoData : public QSharedData {
public:
    explicit SingerInfoData(SingerIdentifier identifier = {}, QString name = {},
                            QList<SpeakerInfo> speakers = {}, QList<LanguageInfo> languages = {},
                            QString defaultLanguage = {}, QString defaultDict = {});
    SingerInfoData(const SingerInfoData &other);
    ~SingerInfoData();

    SingerIdentifier identifier;
    QString name;
    QList<SpeakerInfo> speakers;
    QList<LanguageInfo> languages;
    QString defaultLanguage;
    QString defaultDict;

    bool operator==(const SingerInfoData &other) const;
    bool operator!=(const SingerInfoData &other) const;

    bool isEmpty() const;
};

void swap(SingerInfo &first, SingerInfo &second) noexcept;

Q_DECLARE_METATYPE(SingerInfo)

#endif // SINGERINFO_H