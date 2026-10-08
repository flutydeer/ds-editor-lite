#ifndef PACKAGEINFO_H
#define PACKAGEINFO_H

#include <QVersionNumber>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QDir>
#include <QList>
#include <QSharedData>
#include <QSharedDataPointer>
#include <QVariant>
#include <utility>

#include <lite/ProjectModel/Voice/SingerInfo.h>
#include <lite/Support/LocalizedTextUtils.h>

class PackageInfoData;

class PackageInfo {
public:
    PackageInfo();
    PackageInfo(QString id, QVersionNumber version = {}, QString vendor = {},
                QString description = {}, QString license = {}, QString readme = {},
                QString url = {}, QString path = {}, QList<SingerInfo> singers = {});
    PackageInfo(const PackageInfo &other);
    PackageInfo(PackageInfo &&other) noexcept;
    PackageInfo &operator=(const PackageInfo &other);
    PackageInfo &operator=(PackageInfo &&other) noexcept;

    QString id() const;
    QVersionNumber version() const;
    QString vendor() const;
    QMap<QString, QString> localizedVendor() const;
    [[nodiscard]] QString displayVendor(const QString &bcp47Locale) const;
    [[nodiscard]] QString displayVendor(const QStringList &bcp47Locales) const;
    QString description() const;
    QMap<QString, QString> localizedDescription() const;
    [[nodiscard]] QString displayDescription(const QString &bcp47Locale) const;
    [[nodiscard]] QString displayDescription(const QStringList &bcp47Locales) const;
    QString license() const;
    QMap<QString, QString> localizedLicense() const;
    [[nodiscard]] QString displayLicense(const QString &bcp47Locale) const;
    [[nodiscard]] QString displayLicense(const QStringList &bcp47Locales) const;
    QString readme() const;
    QString url() const;
    QString path() const;
    QList<SingerInfo> singers() const;

    /// Returns the reason why this package could not be opened, or an empty string if the package
    /// loaded.
    ///
    /// A package that fails to open is still listed, so that a user who installed it sees the
    /// reason for the failure instead of a missing entry. The text is the loader's diagnostic,
    /// passed through unchanged, because only the loader has the details of the rejection.
    [[nodiscard]] QString unavailableReason() const;
    [[nodiscard]] bool isUnavailable() const;

    /// Returns whether a package that the loader rejected is listed, based on the rejection
    /// \a reason.
    ///
    /// A rejection without a reason is not listed anywhere, because a row without a reason
    /// provides no explanation. This function is the single point of that decision, so the package
    /// manager and the singer menus always list the same failures.
    [[nodiscard]] static bool isFailureReportable(const QString &reason);

    void setId(const QString &id);
    void setVersion(const QVersionNumber &version);
    void setVendor(const QString &vendor);
    void setLocalizedVendor(const QMap<QString, QString> &names);
    void setDescription(const QString &description);
    void setLocalizedDescription(const QMap<QString, QString> &names);
    void setLicense(const QString &license);
    void setLocalizedLicense(const QMap<QString, QString> &names);
    void setPath(const QString &path);
    void setSingers(const QList<SingerInfo> &singers);
    void setUnavailableReason(const QString &reason);

    void addSinger(const SingerInfo &singer);

    /// Returns the listing of a package that failed to open, built from its directory \a path
    /// and the loader's \a reason.
    ///
    /// The identifier cannot be read from a package that failed to load. The directory name, from
    /// which the packaging tools derive the package name, therefore serves as the title, and the
    /// path identifies the package.
    static PackageInfo unavailable(const QString &path, const QString &reason);

    bool isEmpty() const;

    bool isShared() const;

    void swap(PackageInfo &other) noexcept;

    QString toString() const;

    bool operator==(const PackageInfo &other) const;
    bool operator!=(const PackageInfo &other) const;

private:
    QSharedDataPointer<PackageInfoData> d;
};

class PackageInfoData : public QSharedData {
public:
    explicit PackageInfoData(QString id = {}, QVersionNumber version = {}, QString vendor = {},
                             QString description = {}, QString license = {}, QString readme = {},
                             QString url = {}, QString path = {}, QList<SingerInfo> singers = {});
    PackageInfoData(const PackageInfoData &other);
    ~PackageInfoData();

    QString id;
    QVersionNumber version;
    QString vendor;
    QMap<QString, QString> localizedVendor;
    QString description;
    QMap<QString, QString> localizedDescription;
    QString license;
    QMap<QString, QString> localizedLicense;
    QString readme;
    QString url;
    QString path;
    QString unavailableReason;
    QList<SingerInfo> singers;

    bool operator==(const PackageInfoData &other) const;
    bool operator!=(const PackageInfoData &other) const;

    bool isEmpty() const;
};

void swap(PackageInfo &first, PackageInfo &second) noexcept;

Q_DECLARE_METATYPE(PackageInfo)

#endif // PACKAGEINFO_H
