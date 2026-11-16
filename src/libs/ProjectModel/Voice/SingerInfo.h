#ifndef SINGERINFO_H
#define SINGERINFO_H

#include <QString>
#include <QList>
#include <QStringList>
#include <QMap>
#include <QSharedData>
#include <QSharedDataPointer>
#include <QVariant>
#include <optional>

#include <lite/ProjectModel/Voice/SpeakerInfo.h>
#include <lite/ProjectModel/Voice/LanguageInfo.h>
#include <lite/ProjectModel/AppModel/SingerIdentifier.h>
#include <lite/Support/LocalizedTextUtils.h>

class SingerInfoData;

// Runtime metadata state, omitted from DSPX:
// - Resolved: Installed metadata is available for G2P routing.
// - Pending: Metadata has yet to be resolved. G2P routing must wait.
// - Missing: No installed voicebank matches the required singer and package version.
enum class ResolutionState {
    Resolved,
    Pending,
    Missing,
};

/// Voicebank capabilities available to the host UI at load time.
///
/// consistency fields use int (0=Ideal, 1=Degraded, 2=Inconsistent) to avoid
/// cross-DLL enum export coupling with synthrt ConsistencyLevel.
struct SingerCapabilitySummary {
    QList<QString> mixableSpeakers; ///< singer-domain speaker names allowed to mix
    int speakerConsistency = 0;     ///< 0=Ideal, 1=Degraded, 2=Inconsistent
    QStringList speakerWarnings;

    std::optional<QStringList> acousticParameters; ///< nullopt when support cannot be determined
    std::optional<bool> pitchUsesExpressiveness;
    std::optional<bool> vocoderPitchControllable;

    QList<QString> effectivePhonemes; ///< non-vocoder stage intersection
    int phonemeConsistency = 0;
    QStringList phonemeWarnings;
    bool phonemeDegraded = false;

    QList<QString> effectiveLanguages; ///< non-vocoder stage intersection
    int languageConsistency = 0;
    QStringList languageWarnings;

    bool operator==(const SingerCapabilitySummary &other) const;
    bool operator!=(const SingerCapabilitySummary &other) const;

    static QString consistencyText(int level);
};

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
    QMap<QString, QString> localizedNames() const;
    [[nodiscard]] QString displayName(const QString &bcp47Locale) const;
    [[nodiscard]] QString displayName(const QStringList &bcp47Locales) const;
    QString singerId() const;
    QString packageId() const;
    QVersionNumber packageVersion() const;
    QList<SpeakerInfo> speakers() const;
    QList<LanguageInfo> languages() const;
    QString g2pId(const QString &language) const;
    QString defaultLanguage() const;
    QString defaultG2pId() const;
    QString defaultDict() const;

    ResolutionState resolutionState() const;

    /// Parse-time capability summary (mixable speakers / effective phonemes /
    /// languages). nullopt for pure G2P packages or legacy singers.
    const std::optional<SingerCapabilitySummary> &capability() const;
    void setCapability(std::optional<SingerCapabilitySummary> cap);

    void setIdentifier(const SingerIdentifier &identifier);
    void setName(const QString &name);
    void setLocalizedNames(const QMap<QString, QString> &names);
    void setSpeakers(const QList<SpeakerInfo> &speakers);
    void setLanguages(const QList<LanguageInfo> &languages);
    void setDefaultLanguage(const QString &defaultLanguage);
    void setDefaultDict(const QString &defaultDict);
    void setResolutionState(ResolutionState state);

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
    QMap<QString, QString> localizedNames;
    QList<SpeakerInfo> speakers;
    QList<LanguageInfo> languages;
    QString defaultLanguage;
    QString defaultDict;
    // A singer remains pending until its installed metadata is checked.
    ResolutionState resolutionState = ResolutionState::Pending;

    std::optional<SingerCapabilitySummary> capability;

    bool operator==(const SingerInfoData &other) const;
    bool operator!=(const SingerInfoData &other) const;

    bool isEmpty() const;
};

void swap(SingerInfo &first, SingerInfo &second) noexcept;

Q_DECLARE_METATYPE(SingerInfo)

#endif // SINGERINFO_H
