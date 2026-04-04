//
// Created by FlutyDeer on 2026/4/1.
//

#ifndef DS_EDITOR_LITE_PHONEMENAMELISTMODEL_H
#define DS_EDITOR_LITE_PHONEMENAMELISTMODEL_H

#include <QAbstractListModel>

#include "PhonemeNameItemModel.h"

class PhonemeNameListModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit PhonemeNameListModel(QObject *parent = nullptr);

    enum Roles {
        LanguageRole = Qt::UserRole + 1,
        NameRole,
        IsOnsetRole
    };

    [[nodiscard]] PhonemeNameItemModel getItem(const QModelIndex &index) const;
    [[nodiscard]] QList<PhonemeNameItemModel> items() const;
    void setItems(const QList<PhonemeNameItemModel> &items);
    void addItem(const PhonemeNameItemModel &item);
    void insertItem(int row, const PhonemeNameItemModel &item);
    void removeItem(int row);
    void updateItem(int row, const PhonemeNameItemModel &item);

    [[nodiscard]] int rowCount(const QModelIndex &parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

private:
    QList<PhonemeNameItemModel> m_items;
};



#endif //DS_EDITOR_LITE_PHONEMENAMELISTMODEL_H
