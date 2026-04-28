//
// Created by Trae AI on 2025/4/28.
//

#include "PackageManagerViewModel.h"

#include "Modules/PackageManager/Models/PackageInfo.h"

#include <QDesktopServices>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <qtconcurrentrun.h>

PackageManagerViewModel::PackageManagerViewModel(QObject *parent) : QObject(parent) {
}

bool PackageManagerViewModel::hasPackage() const {
    return m_hasPackage;
}

QString PackageManagerViewModel::packageId() const {
    return m_packageId;
}

QString PackageManagerViewModel::vendor() const {
    return m_vendor;
}

QString PackageManagerViewModel::version() const {
    return m_version;
}

QString PackageManagerViewModel::copyright() const {
    return m_copyright;
}

QString PackageManagerViewModel::description() const {
    return m_description;
}

QString PackageManagerViewModel::websiteUrl() const {
    return m_websiteUrl;
}

QString PackageManagerViewModel::readMeContent() const {
    return m_readMeContent;
}

bool PackageManagerViewModel::readMeLoading() const {
    return m_readMeLoading;
}

void PackageManagerViewModel::setPackage(const PackageInfo *package) {
    if (m_readMeWatcher) {
        m_readMeWatcher->cancel();
        m_readMeWatcher->deleteLater();
        m_readMeWatcher = nullptr;
    }

    if (!package) {
        m_hasPackage = false;
        m_packageId.clear();
        m_vendor.clear();
        m_version.clear();
        m_copyright.clear();
        m_description.clear();
        m_websiteUrl.clear();
        m_readMeContent.clear();
        m_readMeLoading = false;
        emit packageChanged();
        emit readMeContentChanged();
        emit readMeLoadingChanged();
        return;
    }

    m_hasPackage = true;
    m_packageId = package->id();
    m_vendor = package->vendor();
    m_version = "v" + package->version().toString();
    m_copyright = package->copyright();
    m_description = package->description();
    m_websiteUrl = package->url();
    emit packageChanged();

    if (package->readme().isEmpty()) {
        m_readMeContent.clear();
        m_readMeLoading = false;
        emit readMeContentChanged();
        emit readMeLoadingChanged();
    } else {
        QFileInfo readmeFileInfo(package->path(), package->readme());
        loadReadMe(readmeFileInfo.absoluteFilePath());
    }
}

void PackageManagerViewModel::openWebsite() const {
    if (!m_websiteUrl.isEmpty())
        QDesktopServices::openUrl(QUrl(m_websiteUrl));
}

void PackageManagerViewModel::verify() {
    // TODO: implement verify logic
}

void PackageManagerViewModel::uninstall() {
    // TODO: implement uninstall logic
}

void PackageManagerViewModel::loadReadMe(const QString &path) {
    m_readMeLoading = true;
    m_readMeContent = tr("Loading...");
    emit readMeLoadingChanged();
    emit readMeContentChanged();

    m_readMeWatcher = new QFutureWatcher<QString>(this);
    connect(m_readMeWatcher, &QFutureWatcher<QString>::finished, this, [this]() {
        m_readMeContent = m_readMeWatcher->result();
        m_readMeLoading = false;
        emit readMeContentChanged();
        emit readMeLoadingChanged();
        m_readMeWatcher->deleteLater();
        m_readMeWatcher = nullptr;
    });

    QFuture<QString> future = QtConcurrent::run([path]() {
        QFile file(path);
        if (file.size() > 64 * 1024)
            return QObject::tr("File is too large to read.");
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QString content = in.readAll();
            file.close();
            return content;
        }
        return QObject::tr("Failed to open file.");
    });

    m_readMeWatcher->setFuture(future);
}
