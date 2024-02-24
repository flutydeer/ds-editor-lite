#ifndef DS_EDITOR_LITE_PHONICDELEGATE_H
#define DS_EDITOR_LITE_PHONICDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QDebug>
#include <QTableView>
#include <QStandardItemModel>

namespace FillLyric {
    class PhonicDelegate final: public QStyledItemDelegate {
        Q_OBJECT
    public:
        explicit PhonicDelegate(QObject *parent = nullptr);

        void setModelData(QWidget *editor, QAbstractItemModel *model,
                          const QModelIndex &index) const override;

        void paint(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

        void setFontSizeDiff(const int &diff);

    Q_SIGNALS:
        void setToolTip(QModelIndex index) const;
        void clearToolTip(QModelIndex index) const;
        void lyricEdited(QModelIndex index, const QString &text) const;

    private:
        int fontSizeDiff = 3;
    };
}


#endif // DS_EDITOR_LITE_PHONICDELEGATE_H
