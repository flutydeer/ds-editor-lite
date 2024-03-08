#ifndef DS_EDITOR_LITE_LYRICWIDGET_H
#define DS_EDITOR_LITE_LYRICWIDGET_H

#include <QObject>
#include <QCheckBox>

#include "../Controls/PhonicTextEdit.h"
#include "../Controls/PhonicWidget.h"
#include "../Widgets/TableConfigWidget.h"

#include "UI/Controls/Button.h"
#include "UI/Controls/ComboBox.h"

#include "../../Model/PhonicCommon.h"

class LineEdit;

namespace FillLyric {
    class LyricTab final : public QWidget {
        Q_OBJECT
    public:
        explicit LyricTab(QList<Phonic *> phonics, QWidget *parent = nullptr);
        ~LyricTab() override;

        void setPhonics();
        [[nodiscard]] QList<Phonic> exportPhonics() const;

        [[nodiscard]] QList<Phonic> splitLyric(const QString &lyric) const;
        [[nodiscard]] QList<Phonic> modelExport() const;

        QCheckBox *exportSkipSlur;
        QCheckBox *exportExcludeSpace;

    Q_SIGNALS:
        void shrinkWindowRight(int newWidth);
        void expandWindowRight();

    public Q_SLOTS:
        // Buttons
        void _on_btnInsertText_clicked() const;
        void _on_btnToTable_clicked() const;
        void _on_btnToText_clicked() const;
        void _on_btnImportLrc_clicked();

        void _on_splitComboBox_currentIndexChanged(int index) const;

        // count
        void _on_textEditChanged() const;
        void _on_modelDataChanged() const;

    private:
        QList<Phonic *> m_phonics;

        // Variables
        int notesCount = 0;

        // Layout
        QVBoxLayout *m_mainLayout;
        QHBoxLayout *m_tableTopLayout;
        QHBoxLayout *m_lyricLayout;

        QWidget *m_textEditWidget;
        QVBoxLayout *m_textEditLayout;
        QHBoxLayout *m_textTopLayout;
        QHBoxLayout *m_textBottomLayout;
        QHBoxLayout *m_skipSlurLayout;

        QWidget *m_lyricOptWidget;
        QVBoxLayout *m_lyricOptLayout;

        QWidget *m_tableWidget;
        QVBoxLayout *m_tableLayout;
        QHBoxLayout *m_tableCountLayout;
        QHBoxLayout *m_tableBottomLayout;

        // Widgets
        PhonicTextEdit *m_textEdit;
        PhonicWidget *m_phonicWidget;
        TableConfigWidget *m_tableConfigWidget;

        // Labels
        QLabel *m_textCountLabel;
        QLabel *noteCountLabel;
        QLabel *splitLabel;

        // Buttons
        Button *btnInsertText;
        Button *btnToTable;
        Button *btnToText;

        // textEditTop
        Button *btnImportLrc;
        Button *btnReReadNote;
        Button *btnLyricPrev;

        Button *btnFoldLeft;
        Button *btnToggleFermata;
        QCheckBox *autoWrap;
        QPushButton *btnUndo;
        QPushButton *btnRedo;
        QPushButton *btnTableConfig;

        // CheckBox
        QCheckBox *skipSlur;
        QCheckBox *excludeSpace;

        QLabel *exportLabel;

        ComboBox *splitComboBox;
        Button *btnRegSetting;

        // LineEdit
        LineEdit *m_splitters;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_LYRICWIDGET_H
