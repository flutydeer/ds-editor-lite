//
// Created by FlutyDeer on 2025/7/31.
//

#ifndef PACKAGELISTMODEL_H
#define PACKAGELISTMODEL_H

#include <QAbstractListModel>

namespace srt {
    class PackageRef;
}

class PackageListModel : public QAbstractListModel {
public:
    explicit PackageListModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    void setPackages(const QList<srt::PackageRef> &packages);

    int rowCount(const QModelIndex &parent) const override;

    QVariant data(const QModelIndex &index, int role) const override;

    const srt::PackageRef &getPackage(const QModelIndex &index) const;

private:
    QList<srt::PackageRef> m_packages;
};
#endif //PACKAGELISTMODEL_H
