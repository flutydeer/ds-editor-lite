//
// Created by Trae AI on 2025/4/28.
//

#ifndef PACKAGEMANAGERVIEWMODEL_H
#define PACKAGEMANAGERVIEWMODEL_H

#include <QObject>
#include <QFutureWatcher>

class PackageInfo;

class PackageManagerViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool hasPackage READ hasPackage NOTIFY packageChanged)
    Q_PROPERTY(QString packageId READ packageId NOTIFY packageChanged)
    Q_PROPERTY(QString vendor READ vendor NOTIFY packageChanged)
    Q_PROPERTY(QString version READ version NOTIFY packageChanged)
    Q_PROPERTY(QString copyright READ copyright NOTIFY packageChanged)
    Q_PROPERTY(QString description READ description NOTIFY packageChanged)
    Q_PROPERTY(QString websiteUrl READ websiteUrl NOTIFY packageChanged)
    Q_PROPERTY(QString readMeContent READ readMeContent NOTIFY readMeContentChanged)
    Q_PROPERTY(bool readMeLoading READ readMeLoading NOTIFY readMeLoadingChanged)

public:
    explicit PackageManagerViewModel(QObject *parent = nullptr);

    [[nodiscard]] bool hasPackage() const;
    [[nodiscard]] QString packageId() const;
    [[nodiscard]] QString vendor() const;
    [[nodiscard]] QString version() const;
    [[nodiscard]] QString copyright() const;
    [[nodiscard]] QString description() const;
    [[nodiscard]] QString websiteUrl() const;
    [[nodiscard]] QString readMeContent() const;
    [[nodiscard]] bool readMeLoading() const;

    void setPackage(const PackageInfo *package);

    Q_INVOKABLE void openWebsite() const;
    Q_INVOKABLE void verify();
    Q_INVOKABLE void uninstall();

signals:
    void packageChanged();
    void readMeContentChanged();
    void readMeLoadingChanged();

private:
    void loadReadMe(const QString &path);

    bool m_hasPackage = false;
    QString m_packageId;
    QString m_vendor;
    QString m_version;
    QString m_copyright;
    QString m_description;
    QString m_websiteUrl;
    QString m_readMeContent;
    bool m_readMeLoading = false;
    QFutureWatcher<QString> *m_readMeWatcher = nullptr;
};

#endif // PACKAGEMANAGERVIEWMODEL_H
