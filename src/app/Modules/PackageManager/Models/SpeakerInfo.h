#ifndef SPEAKERINFO_H
#define SPEAKERINFO_H

#include <QString>
#include <QSharedData>
#include <QSharedDataPointer>
#include <QMetaType>

#include <optional>
#include <utility>

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

    /// Tone range as {min, max} MIDI note numbers. nullopt when the singer
    /// config does not declare a tone range. Authoritative source mirrored
    /// from synthrt SpeakerInfo.toneRange. The QString toneMin/toneMax fields
    /// are kept in sync for backward-compat with the legacy serialization.
    std::optional<std::pair<int, int>> toneRange() const;
    void setToneRange(std::optional<std::pair<int, int>> range);

    /// True if this speaker is in the singer's mixableSpeakers (i.e. present
    /// in every non-vocoder stage per the parse-time capabilityReport). False
    /// when capabilityReport is nullopt (pure G2P / legacy) — the host should
    /// treat false as "unknown, allow with warning" rather than "not mixable".
    bool mixable() const;
    void setMixable(bool mixable);

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
    std::optional<std::pair<int, int>> toneRange;
    bool mixable = false;
};

void swap(SpeakerInfo &first, SpeakerInfo &second) noexcept;

Q_DECLARE_METATYPE(SpeakerInfo)

#endif // SPEAKERINFO_H
