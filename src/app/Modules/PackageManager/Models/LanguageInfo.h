#ifndef LANGUAGEINFO_H
#define LANGUAGEINFO_H

#include <QString>
#include <QSharedData>
#include <QSharedDataPointer>
#include <QMetaType>

class LanguageInfoData;

class LanguageInfo {
public:
    LanguageInfo();
    explicit LanguageInfo(QString id, QString name = {}, QString g2p = {}, QString dict = {},
                          QString s2pMode = {}, QString onsetMode = {}, QString s2pFile = {},
                          QString onsetFile = {});

    LanguageInfo(const LanguageInfo &other);
    LanguageInfo(LanguageInfo &&other) noexcept;
    LanguageInfo &operator=(const LanguageInfo &other);
    LanguageInfo &operator=(LanguageInfo &&other) noexcept;

    bool operator==(const LanguageInfo &other) const;
    bool operator!=(const LanguageInfo &other) const;

    QString id() const;
    QString name() const;
    QString g2p() const;
    QString dict() const;
    QString s2pMode() const;
    QString onsetMode() const;
    QString s2pFile() const;
    QString onsetFile() const;

    void setId(const QString &id);
    void setName(const QString &name);
    void setG2p(const QString &g2p);
    void setDict(const QString &dict);
    void setS2pMode(const QString &s2pMode);
    void setOnsetMode(const QString &onsetMode);
    void setS2pFile(const QString &s2pFile);
    void setOnsetFile(const QString &onsetFile);

    bool isEmpty() const;

    bool isShared() const;

    void swap(LanguageInfo &other) noexcept;

    QString toString() const;

private:
    QSharedDataPointer<LanguageInfoData> d;
};

class LanguageInfoData : public QSharedData {
public:
    LanguageInfoData();
    explicit LanguageInfoData(QString id, QString name = {}, QString g2p = {}, QString dict = {},
                              QString s2pMode = {}, QString onsetMode = {},
                              QString s2pFile = {}, QString onsetFile = {});
    LanguageInfoData(const LanguageInfoData &other);
    ~LanguageInfoData();

    bool operator==(const LanguageInfoData &other) const;
    bool operator!=(const LanguageInfoData &other) const;

    bool isEmpty() const;

    QString id;
    QString name;
    QString g2p;
    QString dict;
    QString s2pMode;
    QString onsetMode;
    QString s2pFile;
    QString onsetFile;
};

void swap(LanguageInfo &first, LanguageInfo &second) noexcept;

Q_DECLARE_METATYPE(LanguageInfo)

#endif // LANGUAGEINFO_H
