#ifndef DS_EDITOR_LITE_SYLLABLEDELEGATE_H
#define DS_EDITOR_LITE_SYLLABLEDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QDebug>
#include <QTableView>
#include <QStandardItemModel>

class SyllableDelegate : public QStyledItemDelegate {
public:
    explicit SyllableDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        QStyleOptionViewItem newOption(option);
        newOption.displayAlignment = Qt::AlignBottom | Qt::AlignHCenter;
        QStyledItemDelegate::paint(painter, newOption, index);

        // 获取注音文本
        QString syllable = index.data(Qt::UserRole).toString();

        // 设置注音的字体
        QFont syllableFont = option.font;
        syllableFont.setPointSize(syllableFont.pointSize() - 2); // 缩小字体
        painter->setFont(syllableFont);

        // 获取候选发音列表
        QStringList candidateList = index.data(Qt::UserRole + 1).toStringList();
        // 若候选发音大于1个，注音颜色为红色
        if (candidateList.size() > 1) {
            painter->setPen(Qt::red);
        } else {
            painter->setPen(Qt::black);
        }
        // 文字绘制在原字体的上方
        painter->drawText(option.rect, Qt::AlignTop | Qt::AlignHCenter, syllable);
    }
};



#endif // DS_EDITOR_LITE_SYLLABLEDELEGATE_H
