#ifndef LYRICBASEWIDGET_H
#define LYRICBASEWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QCheckBox>

#include "UI/Controls/Button.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Controls/LineEdit.h"

#include "../../../Model/PhonicCommon.h"
#include "../../Controls/PhonicTextEdit.h"

namespace FillLyric {

    class LyricBaseWidget final : public QWidget {
        Q_OBJECT
        friend class LyricTab;

    public:
        explicit LyricBaseWidget(QWidget *parent = nullptr);
        ~LyricBaseWidget() override;

        [[nodiscard]] QList<Phonic> splitLyric(const QString &lyric) const;

    public Q_SLOTS:
        void _on_btnImportLrc_clicked();
        void _on_textEditChanged() const;
        void _on_splitComboBox_currentIndexChanged(int index) const;

    private:
        QVBoxLayout *m_textEditLayout;
        QHBoxLayout *m_textTopLayout;
        QHBoxLayout *m_textBottomLayout;

        QWidget *m_splitWidget;
        QHBoxLayout *m_splitLayout;
        QHBoxLayout *m_skipSlurLayout;

        QLabel *m_textCountLabel;
        PhonicTextEdit *m_textEdit;

        // textEditTop
        Button *btnImportLrc;
        Button *btnReReadNote;
        Button *btnLyricPrev;

        // CheckBox
        QCheckBox *skipSlur;

        QLabel *m_splitLabel;
        ComboBox *m_splitComboBox;
        LineEdit *m_splitters;
    };


} // FillLyric

#endif // LYRICBASEWIDGET_H
