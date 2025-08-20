//
// Created by FlutyDeer on 2025/7/31.
//

#include "PackageListModel.h"

#include "synthrt/Core/PackageRef.h"

const PackageInfo &PackageListModel::getPackage(const QModelIndex &index) const {
    return m_packages.at(index.row());
}

void PackageListModel::setPackages(QList<PackageInfo> packages) {
    beginResetModel();
    m_packages = std::move(packages);
    endResetModel();
}

int PackageListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_packages.size();
}

QVariant PackageListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_packages.size())
        return {};

    const auto &package = m_packages.at(index.row());
    if (role == Qt::UserRole) {  // 用于过滤的原始数据
        return QVariant::fromValue(package);
    }
    return {};
}
