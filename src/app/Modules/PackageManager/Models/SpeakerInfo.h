#ifndef SPEAKERINFO_H
#define SPEAKERINFO_H

#include <QString>
#include <QSharedData>
#include <QSharedDataPointer>
#include <QMetaType>

class SpeakerInfoData;

class SpeakerInfo {
public:
    SpeakerInfo();
    explicit SpeakerInfo(QString id);
    SpeakerInfo(QString id, QString name);
    SpeakerInfo(QString id, QString name, QString toneMin, QString toneMax);

    SpeakerInfo(const SpeakerInfo &other);
    SpeakerInfo(SpeakerInfo &&other) noexcept;
    SpeakerInfo &operator=(const SpeakerInfo &other);
    SpeakerInfo &operator=(SpeakerInfo &&other) noexcept;

    bool operator==(const SpeakerInfo &other) const;
    bool operator!=(const SpeakerInfo &other) const;

    QString id() const;
    QString name() const;
    QString toneMin() const;
    QString toneMax() const;

    void setId(const QString &id);
    void setName(const QString &name);
    void setToneMin(const QString &toneMin);
    void setToneMax(const QString &toneMax);

    bool isEmpty() const;

    bool isShared() const;

    void swap(SpeakerInfo &other) noexcept;

    QString toString() const;

private:
    QSharedDataPointer<SpeakerInfoData> d;
};

class SpeakerInfoData : public QSharedData {
public:
    SpeakerInfoData();
    explicit SpeakerInfoData(QString id);
    SpeakerInfoData(QString id, QString name);
    SpeakerInfoData(QString id, QString name, QString toneMin, QString toneMax);
    SpeakerInfoData(const SpeakerInfoData &other);
    ~SpeakerInfoData();

    bool operator==(const SpeakerInfoData &other) const;
    bool operator!=(const SpeakerInfoData &other) const;

    bool isEmpty() const;

    QString id;
    QString name;
    QString toneMin;
    QString toneMax;
};

void swap(SpeakerInfo &first, SpeakerInfo &second) noexcept;

Q_DECLARE_METATYPE(SpeakerInfo)

#endif // SPEAKERINFO_H