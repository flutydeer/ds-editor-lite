//
// Created by FlutyDeer on 2025/7/31.
//

#include "PackageListModel.h"

#include "synthrt/Core/PackageRef.h"

void PackageListModel::setPackages(const QList<srt::PackageRef> &packages) {
    beginResetModel();
    m_packages = packages;
    endResetModel();
}

int PackageListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_packages.size();
}

QVariant PackageListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_packages.size())
        return QVariant();

    const auto &package = m_packages.at(index.row());
    if (role == Qt::UserRole) {  // 用于过滤的原始数据
        return QVariant::fromValue(package);
    }
    return QVariant();
}

const srt::PackageRef & PackageListModel::getPackage(const QModelIndex &index) const {
    return m_packages.at(index.row());
}