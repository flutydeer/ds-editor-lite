#include "PhonicDelegate.h"

namespace FillLyric {
    void PhonicDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                      const QModelIndex &index) const {
        if (editor->property("text").toString() != model->data(index).toString()) {
            Q_EMIT this->lyricEdited(index, editor->property("text").toString());
        }
    }

    void PhonicDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                               const QModelIndex &index) const {
        // 获取原文本
        QString text = index.data(Qt::DisplayRole).toString();

        // 获取注音文本
        QString syllable = index.data(PhonicRole::Syllable).toString();

        // 获取人工修订的注音文本
        QString syllableRevised = index.data(PhonicRole::SyllableRevised).toString();

        auto lyricType = index.data(PhonicRole::LyricType).toInt();

        // 获取候选发音列表
        QStringList candidateList = index.data(PhonicRole::Candidate).toStringList();
        // 若候选发音大于1个，注音颜色为红色
        if (syllableRevised != "") {
            painter->setPen(Qt::blue);
            syllable = syllableRevised;
        } else if (text == syllable && lyricType != LyricType::Slur) {
            painter->setPen(Qt::darkBlue);
        } else if (candidateList.size() > 1) {
            painter->setPen(QColor("#9BBAFF"));
        } else {
            painter->setPen(QColor("#F0F0F0"));
        }

        // 获取小三个字号的字体的高
        QFont textFont(option.font);
        int textFontHeight = QFontMetrics(textFont).height();
        int textFontXHeight = QFontMetrics(textFont).xHeight();

        auto textWidth = textFontXHeight * text.size();

        if (textWidth > (int) (option.rect.width() * 0.8)) {
            textFont.setPointSize(
                (int) (textFont.pointSize() * option.rect.width() * 0.8 / (double) textWidth));
        }

        // 绘制歌词文本
        auto yOffset = textFontHeight / 2;
        painter->setFont(textFont);
        QRect textRect = option.rect.adjusted(0, yOffset, 0, yOffset);

        painter->drawText(textRect, Qt::AlignCenter, text);

        // 注音文本
        QRect syllableRect = option.rect.adjusted(0, -yOffset, 0, -yOffset);
        QFont syllableFont = textFont;
        syllableFont.setPointSize(syllableFont.pointSize() - 2);
        painter->setFont(syllableFont);
        painter->drawText(syllableRect, Qt::AlignCenter, syllable);

        // 字体恢复
        painter->setFont(textFont);
    }
}