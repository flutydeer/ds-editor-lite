//
// Created by FlutyDeer on 2025/7/31.
//

#ifndef PACKAGEFILTERPROXYMODEL_H
#define PACKAGEFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>

class PackageFilterProxyModel : public QSortFilterProxyModel {
public:
    explicit PackageFilterProxyModel(QObject *parent = nullptr);

    void setFilterString(const QString &pattern);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_filterPattern;
};

#endif // PACKAGEFILTERPROXYMODEL_H
