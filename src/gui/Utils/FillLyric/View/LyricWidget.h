#ifndef DS_EDITOR_LITE_LYRICWIDGET_H
#define DS_EDITOR_LITE_LYRICWIDGET_H

#include <QObject>
#include <QWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "PhonicTextEdit.h"
#include "PhonicWidget.h"
#include "TableConfigWidget.h"

#include "Controls/Button.h"
#include "Controls/ComboBox.h"

#include "../Utils/CleanLyric.h"
#include "../Model/PhonicCommon.h"

class LineEdit;

namespace FillLyric {
    class LyricWidget : public QWidget {
        Q_OBJECT
    public:
        explicit LyricWidget(QList<PhonicNote *> phonicNotes, QWidget *parent = nullptr);
        ~LyricWidget() override;

    Q_SIGNALS:
        void shrinkWindowRight(int newWidth);
        void expandWindowRight();

    public Q_SLOTS:
        // Buttons
        void _on_btnInsertText_clicked();
        void _on_btnToTable_clicked();
        void _on_btnToText_clicked();
        void _on_btnImportLrc_clicked();

        void _on_splitComboBox_currentIndexChanged(int index);

        // count
        void _on_textEditChanged();
        void _on_modelDataChanged();

    private:
        QList<PhonicNote *> m_phonicNotes;

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
        Button *btnImportLrc;
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

        ComboBox *splitComboBox;
        Button *btnRegSetting;

        // LineEdit
        LineEdit *m_splitters;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_LYRICWIDGET_H
