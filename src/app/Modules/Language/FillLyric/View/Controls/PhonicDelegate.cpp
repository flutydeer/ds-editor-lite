#include "PhonicDelegate.h"

#include "../../Utils/SplitLyric.h"
#include "../../Model/PhonicCommon.h"

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
        QString text = index.data(Qt::DisplayRole).toString();
        QString syllable = index.data(Syllable).toString();
        const QString syllableRevised = index.data(SyllableRevised).toString();
        const QString lyricType = index.data(Language).toString();
        const QStringList candidateList = index.data(Candidate).toStringList();

        // Label colors based on analysis results.
        if (syllableRevised != "") {
            painter->setPen(QColor(255, 204, 153));
            syllable = syllableRevised;
        } else if (text == syllable && lyricType != "Slur") {
            painter->setPen(QColor(255, 155, 157));
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

        const int textFontHeight = QFontMetrics(textFont).height();
        const int textFontXHeight = QFontMetrics(textFont).xHeight();
        const int syllableFontXHeight = QFontMetrics(syllableFont).xHeight();

        const int delegateWidth = static_cast<int>(option.rect.width() * 0.9);
        const int maxTextSize = delegateWidth / textFontXHeight - 3;
        const int maxSyllableSize = delegateWidth / syllableFontXHeight - 3;

        const bool addToolTip = text.size() > 1 && text.size() > maxTextSize && maxTextSize > 0;
        const QString cellToolTip = index.data(Qt::ToolTipRole).toString();
        if (!cellToolTip.isEmpty() && !addToolTip) {
            Q_EMIT this->clearToolTip(index);
        }

        // When the text is too long, display ellipses.
        if (addToolTip) {
            text = text.left(maxTextSize) + "...";
            Q_EMIT this->setToolTip(index);
        }

        // When Zhuyin is too long, display ellipsis.
        if (syllable.size() > 1 && syllable.size() > maxSyllableSize && maxSyllableSize > 0) {
            syllable = syllable.left(maxSyllableSize) + "...";
        }

        // Draw Lyrics Text
        const int yOffset = textFontHeight / 2;
        painter->setFont(textFont);
        const QRect textRect = option.rect.adjusted(0, yOffset, 0, yOffset);

        painter->drawText(textRect, Qt::AlignCenter, text);

        // Draw Syllable Text
        const QRect syllableRect = option.rect.adjusted(0, -yOffset, 0, -yOffset);

        painter->setFont(syllableFont);
        painter->drawText(syllableRect, Qt::AlignCenter, syllable);

        // Restore font size
        painter->setFont(textFont);
    }

    void PhonicDelegate::setFontSizeDiff(const int &diff) {
        this->fontSizeDiff = diff;
    }
}