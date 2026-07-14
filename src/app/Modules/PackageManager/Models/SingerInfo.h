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

// SingerInfo 解析状态（显式 pending 状态）
// - Resolved: 完整元数据，G2P 可路由（PackageManager 命中）
// - Pending: 待补全，G2P 暂不可路由（dspx fallback 构造，PackageManager 未就绪）
// - Missing: 声库未安装/版本不匹配，永久不可路由（异步补全后仍未命中）
// 注：运行期状态，不持久化到 dspx
enum class ResolutionState {
    Resolved,
    Pending,
    Missing,
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

    void setIdentifier(const SingerIdentifier &identifier);
    void setName(const QString &name);
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
    QList<SpeakerInfo> speakers;
    QList<LanguageInfo> languages;
    QString defaultLanguage;
    QString defaultDict;
    // 默认 Pending：未经验证的 SingerInfo 不应被标记为已解析。
    // PackageManager 加载成功后显式 setResolutionState(Resolved)。
    ResolutionState resolutionState = ResolutionState::Pending;

    bool operator==(const SingerInfoData &other) const;
    bool operator!=(const SingerInfoData &other) const;

    bool isEmpty() const;
};

void swap(SingerInfo &first, SingerInfo &second) noexcept;

Q_DECLARE_METATYPE(SingerInfo)

#endif // SINGERINFO_H