//
// Created by FlutyDeer on 2025/7/31.
//

#ifndef PACKAGELISTMODEL_H
#define PACKAGELISTMODEL_H

#include <QAbstractListModel>

#include "Modules/PackageManager/Models/PackageInfo.h"

class PackageListModel : public QAbstractListModel {
public:
    explicit PackageListModel(QObject *parent = nullptr) : QAbstractListModel(parent) {
    }

    const PackageInfo &getPackage(const QModelIndex &index) const;
    void setPackages(QList<PackageInfo> packages);

    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

private:
    QList<PackageInfo> m_packages;
};
#endif // PACKAGELISTMODEL_H
