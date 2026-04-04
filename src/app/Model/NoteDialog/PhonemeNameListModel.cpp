//
// Created by FlutyDeer on 2026/4/1.
//


#include "PhonemeNameListModel.h"

PhonemeNameListModel::PhonemeNameListModel(QObject *parent) : QAbstractListModel(parent) {
}

PhonemeNameItemModel PhonemeNameListModel::getItem(const QModelIndex &index) const {
    return m_items.at(index.row());
}

QList<PhonemeNameItemModel> PhonemeNameListModel::items() const {
    return m_items;
}

void PhonemeNameListModel::setItems(const QList<PhonemeNameItemModel> &items) {
    beginResetModel();
    m_items = items;
    endResetModel();
}

void PhonemeNameListModel::addItem(const PhonemeNameItemModel &item) {
    beginInsertRows({}, m_items.size(), m_items.size());
    m_items.append(item);
    endInsertRows();
}

void PhonemeNameListModel::insertItem(int row, const PhonemeNameItemModel &item) {
    if (row < 0 || row > m_items.size())
        return;
    beginInsertRows({}, row, row);
    m_items.insert(row, item);
    endInsertRows();
}

void PhonemeNameListModel::removeItem(int row) {
    if (row < 0 || row >= m_items.size())
        return;
    beginRemoveRows({}, row, row);
    m_items.removeAt(row);
    endRemoveRows();
}

void PhonemeNameListModel::updateItem(int row, const PhonemeNameItemModel &item) {
    if (row < 0 || row >= m_items.size())
        return;
    m_items[row] = item;
    emit dataChanged(index(row), index(row));
}

int PhonemeNameListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_items.size();
}

QVariant PhonemeNameListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_items.size())
        return {};

    const auto &item = m_items.at(index.row());
    switch (role) {
        case LanguageRole:
            return item.language();
        case NameRole:
            return item.name();
        case IsOnsetRole:
            return item.isOnset();
        default:
            return {};
    }
}

bool PhonemeNameListModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (!index.isValid() || index.row() >= m_items.size())
        return false;

    auto &item = m_items[index.row()];
    switch (role) {
        case LanguageRole:
            item.setLanguage(value.toString());
            break;
        case NameRole:
            item.setName(value.toString());
            break;
        case IsOnsetRole:
            item.setIsOnset(value.toBool());
            break;
        default:
            return false;
    }
    emit dataChanged(index, index, {role});
    return true;
}
