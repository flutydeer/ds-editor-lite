//
// Created by FlutyDeer on 2025/7/31.
//

#include "PackageFilterProxyModel.h"

#include "synthrt/Core/PackageRef.h"
#include "UI/Dialogs/PackageManager/PackageListModel.h"

PackageFilterProxyModel::PackageFilterProxyModel(QObject *parent): QSortFilterProxyModel(parent) {
}

void PackageFilterProxyModel::setFilterString(const QString &pattern) {
    m_filterPattern = pattern;
    invalidateFilter();
}

bool PackageFilterProxyModel::filterAcceptsRow(
    int sourceRow, const QModelIndex &sourceParent) const {
    const auto *model = dynamic_cast<PackageListModel *>(sourceModel());
    const auto &package = model->getPackage(model->index(sourceRow, 0, sourceParent));

    bool idMatch = m_filterPattern.isEmpty() ||
                  package.id.contains(m_filterPattern, Qt::CaseInsensitive);

    bool vendorMatch = m_filterPattern.isEmpty() ||
                      package.vendor.contains(m_filterPattern, Qt::CaseInsensitive);

    return idMatch || vendorMatch;
}