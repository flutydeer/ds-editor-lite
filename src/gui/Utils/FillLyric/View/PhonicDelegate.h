#ifndef DS_EDITOR_LITE_PHONICDELEGATE_H
#define DS_EDITOR_LITE_PHONICDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QDebug>
#include <QTableView>
#include <QStandardItemModel>

#include "../Utils/CleanLyric.h"
#include "../Model/PhonicCommon.h"

namespace FillLyric {
    class PhonicDelegate : public QStyledItemDelegate {
        Q_OBJECT
    public:
        explicit PhonicDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {
        }

        void setModelData(QWidget *editor, QAbstractItemModel *model,
                          const QModelIndex &index) const override;

        void paint(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    Q_SIGNALS:
        void lyricEdited(QModelIndex index, const QString &text) const;
    };
}


#endif // DS_EDITOR_LITE_PHONICDELEGATE_H
