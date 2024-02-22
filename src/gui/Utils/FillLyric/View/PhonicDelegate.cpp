#include "PhonicDelegate.h"

namespace FillLyric {
    PhonicDelegate::PhonicDelegate(QObject *parent) : QStyledItemDelegate(parent) {
    }

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
            painter->setPen(QColor(255, 204, 153));
            syllable = syllableRevised;
        } else if (text == syllable && lyricType != TextType::Slur &&
                   lyricType != TextType::EnWord) {
            painter->setPen(Qt::darkBlue);
        } else if (candidateList.size() > 1) {
            painter->setPen(QColor(155, 186, 255));
        } else {
            painter->setPen(QColor(240, 240, 240));
        }

        QFont textFont(option.font);
        if (text.size() > 1)
            textFont.setPointSize(textFont.pointSize() - fontSizeDiff);
        QFont syllableFont = textFont;
        syllableFont.setPointSize(syllableFont.pointSize() - fontSizeDiff);

        int textFontHeight = QFontMetrics(textFont).height();
        int textFontXHeight = QFontMetrics(textFont).xHeight();
        int syllableFontXHeight = QFontMetrics(syllableFont).xHeight();

        auto delegateWidth = option.rect.width() * 0.9;
        auto maxTextSize = (int) (delegateWidth / textFontXHeight) - 3;
        auto maxSyllableSize = (int) (delegateWidth / syllableFontXHeight) - 3;

        bool addToolTip = text.size() > 1 && text.size() > maxTextSize && maxTextSize > 0;
        QString cellToolTip = index.data(Qt::ToolTipRole).toString();
        if (!cellToolTip.isEmpty() && !addToolTip) {
            Q_EMIT this->clearToolTip(index);
        }

        // 文本过长时，显示省略号
        if (addToolTip) {
            text = text.left(maxTextSize) + "...";
            Q_EMIT this->setToolTip(index);
        }

        // 注音过长时，显示省略号
        if (syllable.size() > 1 && syllable.size() > maxSyllableSize && maxSyllableSize > 0) {
            syllable = syllable.left(maxSyllableSize) + "...";
        }

        // 绘制歌词文本
        auto yOffset = textFontHeight / 2;
        painter->setFont(textFont);
        QRect textRect = option.rect.adjusted(0, yOffset, 0, yOffset);

        painter->drawText(textRect, Qt::AlignCenter, text);

        // 注音文本
        QRect syllableRect = option.rect.adjusted(0, -yOffset, 0, -yOffset);

        painter->setFont(syllableFont);
        painter->drawText(syllableRect, Qt::AlignCenter, syllable);

        // 字体恢复
        painter->setFont(textFont);
    }

    void PhonicDelegate::setFontSizeDiff(int diff) {
        this->fontSizeDiff = diff;
    }
}